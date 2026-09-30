/*
 * grid_sim - runs the PVRouter firmware on a simulated 3-phase grid
 *
 * The production firmware (firmware.elf) runs in simavr, an ATmega328P emulator.
 * grid_sim feeds its 6 ADC channels with 50 Hz voltage and current waves computed
 * at the exact CPU cycle of each conversion, and models the loads driven by the
 * firmware's output pins: when a load conducts, its power is taken from the surplus
 * of its phase, which the firmware then measures.
 *
 * Loads are driven through zero-crossing triac drivers: a pin change only takes
 * effect at the next zero crossing of the load's own phase.
 *
 * Measured: ADC conversion period, ADC ISR duration (cycle-exact), load switching,
 * grid import/export energy, and the firmware's own serial output.
 *
 * Usage: grid_sim [-q] [-t trace.csv] [-e events.csv] firmware.elf scenario.scn
 *
 * Copyright (c) 2026 Frédéric Metrich
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <ctype.h>
#include <libgen.h>
#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "avr_adc.h"
#include "avr_ioport.h"
#include "avr_uart.h"
#include "sim_avr.h"
#include "sim_elf.h"
#include "sim_interrupts.h"

#define F_CPU 16000000UL
#define ADC_VECTOR 21                  /* ADC_vect on the ATmega328P */
#define ADC_CYCLES_PER_CONVERSION 1664 /* 13 ADC clocks at F_CPU / 128 */
#define ADC_MID_COUNTS 511.5           /* bias of the analog front end */
#define ADC_MAX_SWING 510.0            /* counts, either side of the bias */
#define NO_OF_PHASES 3

#define MAX_LOADS 8
#define MAX_POINTS 512
#define MAX_EXPECTS 128
#define MAX_INPUTS 16

/* ------------------------------------------------------------------------- */
/* scenario                                                                    */

typedef struct
{
  double t;
  double surplus[NO_OF_PHASES]; /* W available before the router's loads, > 0 = export */
} point_t;

typedef enum
{
  EXPECT_PIN,      /* expect <t> pin <p> on|off */
  EXPECT_GRID_AVG, /* expect <t0> <t1> grid_avg <min> <max>  (W, import > 0, all phases) */
  EXPECT_ISR_MAX,  /* expect isr_max <cycles> */
  EXPECT_MIN_SETS, /* expect min_sample_sets <n>  (lowest value reported by the firmware) */
  EXPECT_OVERRUNS, /* expect isr_overruns <n>  (ISR calls longer than one conversion, at most) */
} expect_kind_t;

typedef struct
{
  expect_kind_t kind;
  double t0, t1;
  int pin, state;
  double lo, hi;
  int line;
  /* evaluation */
  int done, passed;
  double integral, measured;
} expect_t;

typedef struct
{
  int pin;
  int value;
} input_t;

typedef struct
{
  int pin;
  int phase; /* 0-based */
  double watts;
  /* pin state, as driven by the firmware */
  int state, prev_state;
  avr_cycle_count_t changed_at;
  /* power stage */
  int conducting;
  /* statistics */
  unsigned switches;
  double on_time, energy_wh, first_on;
  /* delay from a pin change to the zero crossing where the power stage follows it */
  unsigned transitions, at_positive_zc;
  double latency_sum, latency_max;
} load_t;

static struct
{
  double duration;
  double frequency, vrms;
  double vcal[NO_OF_PHASES], pcal[NO_OF_PHASES];
  load_t loads[MAX_LOADS];
  int no_of_loads;
  point_t points[MAX_POINTS];
  int no_of_points;
  expect_t expects[MAX_EXPECTS];
  int no_of_expects;
  input_t inputs[MAX_INPUTS];
  int no_of_inputs;
  char name[PATH_MAX];
} scn = {
  .duration = 60.0,
  .frequency = 50.0,
  .vrms = 230.0,
  .vcal = { 0.8151, 0.8184, 0.8195 },
  .pcal = { 0.05, 0.05, 0.05 },
};

static void die(const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  fprintf(stderr, "grid_sim: ");
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
  va_end(ap);
  exit(2);
}

static void parse_scenario(const char *path, int depth)
{
  if (depth > 4)
    die("%s: includes nested too deeply", path);

  FILE *f = fopen(path, "r");
  if (!f)
    die("cannot open scenario '%s'", path);

  char dir[PATH_MAX];
  snprintf(dir, sizeof dir, "%s", path);
  dirname(dir);

  char line[512];
  int lineno = 0;
  while (fgets(line, sizeof line, f))
  {
    ++lineno;
    char *hash = strchr(line, '#');
    if (hash)
      *hash = '\0';

    char cmd[32] = "";
    if (sscanf(line, "%31s", cmd) != 1)
      continue;

    const char *args = strstr(line, cmd) + strlen(cmd);
    int ok = 1;

    if (!strcmp(cmd, "include"))
    {
      char file[256], full[PATH_MAX + 257];
      ok = sscanf(args, "%255s", file) == 1;
      if (ok)
      {
        snprintf(full, sizeof full, "%s/%s", dir, file);
        parse_scenario(full, depth + 1);
      }
    }
    else if (!strcmp(cmd, "duration"))
      ok = sscanf(args, "%lf", &scn.duration) == 1;
    else if (!strcmp(cmd, "mains"))
      ok = sscanf(args, "%lf %lf", &scn.frequency, &scn.vrms) == 2;
    else if (!strcmp(cmd, "vcal"))
      ok = sscanf(args, "%lf %lf %lf", &scn.vcal[0], &scn.vcal[1], &scn.vcal[2]) == 3;
    else if (!strcmp(cmd, "pcal"))
      ok = sscanf(args, "%lf %lf %lf", &scn.pcal[0], &scn.pcal[1], &scn.pcal[2]) == 3;
    else if (!strcmp(cmd, "load"))
    {
      if (scn.no_of_loads == MAX_LOADS)
        die("%s:%d: too many loads", path, lineno);
      load_t *l = &scn.loads[scn.no_of_loads];
      ok = sscanf(args, "%d %d %lf", &l->pin, &l->phase, &l->watts) == 3 && l->pin >= 0 && l->pin <= 13
           && l->phase >= 1 && l->phase <= NO_OF_PHASES;
      l->phase -= 1;
      l->first_on = -1.0;
      if (ok)
        ++scn.no_of_loads;
    }
    else if (!strcmp(cmd, "input"))
    {
      if (scn.no_of_inputs == MAX_INPUTS)
        die("%s:%d: too many inputs", path, lineno);
      input_t *in = &scn.inputs[scn.no_of_inputs];
      ok = sscanf(args, "%d %d", &in->pin, &in->value) == 2 && in->pin >= 0 && in->pin <= 13;
      if (ok)
        ++scn.no_of_inputs;
    }
    else if (!strcmp(cmd, "at"))
    {
      if (scn.no_of_points == MAX_POINTS)
        die("%s:%d: too many points", path, lineno);
      point_t *p = &scn.points[scn.no_of_points];
      ok = sscanf(args, "%lf %lf %lf %lf", &p->t, &p->surplus[0], &p->surplus[1], &p->surplus[2]) == 4;
      if (ok && scn.no_of_points && p->t < scn.points[scn.no_of_points - 1].t)
        die("%s:%d: points must be in chronological order", path, lineno);
      if (ok)
        ++scn.no_of_points;
    }
    else if (!strcmp(cmd, "expect"))
    {
      if (scn.no_of_expects == MAX_EXPECTS)
        die("%s:%d: too many expectations", path, lineno);
      expect_t *e = &scn.expects[scn.no_of_expects];
      char a[32], b[32];
      double t;
      e->line = lineno;
      if (sscanf(args, " isr_max %lf", &e->hi) == 1)
        e->kind = EXPECT_ISR_MAX;
      else if (sscanf(args, " min_sample_sets %lf", &e->lo) == 1)
        e->kind = EXPECT_MIN_SETS;
      else if (sscanf(args, " isr_overruns %lf", &e->hi) == 1)
        e->kind = EXPECT_OVERRUNS;
      else if (sscanf(args, "%lf pin %d %31s", &t, &e->pin, a) == 3 && (!strcmp(a, "on") || !strcmp(a, "off")))
      {
        e->kind = EXPECT_PIN;
        e->t0 = t;
        e->state = !strcmp(a, "on");
      }
      else if (sscanf(args, "%lf %lf %31s %lf %lf", &e->t0, &e->t1, b, &e->lo, &e->hi) == 5 && !strcmp(b, "grid_avg")
               && e->t1 > e->t0)
        e->kind = EXPECT_GRID_AVG;
      else
        ok = 0;
      if (ok)
        ++scn.no_of_expects;
    }
    else
      die("%s:%d: unknown command '%s'", path, lineno, cmd);

    if (!ok)
      die("%s:%d: invalid '%s' line", path, lineno, cmd);
  }
  fclose(f);
}

/* surplus of one phase at time t: linear interpolation between the points */
static double surplus_at(int phase, double t)
{
  if (!scn.no_of_points)
    return 0.0;
  if (t <= scn.points[0].t)
    return scn.points[0].surplus[phase];
  for (int i = 1; i < scn.no_of_points; ++i)
  {
    const point_t *a = &scn.points[i - 1], *b = &scn.points[i];
    if (t < b->t)
      return (b->t == a->t) ? b->surplus[phase]
                            : a->surplus[phase] + (b->surplus[phase] - a->surplus[phase]) * (t - a->t) / (b->t - a->t);
  }
  return scn.points[scn.no_of_points - 1].surplus[phase];
}

/* ------------------------------------------------------------------------- */
/* simulation state                                                            */

static avr_t *avr;
static int quiet;
static FILE *trace_file, *events_file;

static double now(void)
{
  return (double)avr->cycle / F_CPU;
}

static struct
{
  double last_t;
  long half_cycle[NO_OF_PHASES]; /* index of the current half cycle of each phase */
  double export_w[NO_OF_PHASES]; /* current grid export per phase (< 0 = import) */
  double import_wh, export_wh;
  double phase_import_wh[NO_OF_PHASES], phase_export_wh[NO_OF_PHASES];
} grid;

static struct
{
  avr_cycle_count_t last_trigger;
  uint64_t conversions;
  avr_cycle_count_t min_period, max_period;
  int clipped;
} adc = { .min_period = UINT64_MAX };

static struct
{
  avr_cycle_count_t started;
  uint64_t count;
  uint64_t total;
  avr_cycle_count_t max;
  double max_at;
  uint64_t overruns; /* calls longer than one conversion: the next sample may use the wrong channel */
} isr;

static struct
{
  char line[256];
  size_t len;
  int datalog_lines;
  int min_sets, max_sets;
} serial = { .min_sets = INT_MAX, .max_sets = INT_MIN };

/* pin state at a given cycle, from the last change of that pin */
static int load_state_at(const load_t *l, avr_cycle_count_t cycle)
{
  return (l->changed_at <= cycle) ? l->state : l->prev_state;
}

/* advances the power model to the current time */
static void update_grid(void)
{
  const double t = now();
  const double dt = t - grid.last_t;

  /* energy over the elapsed interval, with the conduction states it had */
  if (dt > 0)
  {
    double total_import_w = 0;
    for (int k = 0; k < NO_OF_PHASES; ++k)
    {
      const double import_w = -grid.export_w[k];
      total_import_w += import_w;
      if (import_w > 0)
        grid.phase_import_wh[k] += import_w * dt / 3600.0;
      else
        grid.phase_export_wh[k] -= import_w * dt / 3600.0;
    }
    if (total_import_w > 0)
      grid.import_wh += total_import_w * dt / 3600.0;
    else
      grid.export_wh -= total_import_w * dt / 3600.0;

    for (int i = 0; i < scn.no_of_loads; ++i)
    {
      load_t *l = &scn.loads[i];
      if (l->conducting)
      {
        l->on_time += dt;
        l->energy_wh += l->watts * dt / 3600.0;
      }
    }

    for (int i = 0; i < scn.no_of_expects; ++i)
    {
      expect_t *e = &scn.expects[i];
      if (e->kind != EXPECT_GRID_AVG)
        continue;
      const double a = (grid.last_t > e->t0) ? grid.last_t : e->t0;
      const double b = (t < e->t1) ? t : e->t1;
      if (b > a)
        e->integral += total_import_w * (b - a);
    }
  }
  grid.last_t = t;

  /* zero crossings: phase k is sin(2 pi f t - 2 pi k / 3), so its half cycles
   * are numbered floor(2 f t - 2 k / 3) and start at t = (m + 2 k / 3) / (2 f) */
  for (int k = 0; k < NO_OF_PHASES; ++k)
  {
    const long m = (long)floor(2.0 * scn.frequency * t - 2.0 * k / 3.0);
    if (m == grid.half_cycle[k])
      continue;
    grid.half_cycle[k] = m;

    const double t_zc = (m + 2.0 * k / 3.0) / (2.0 * scn.frequency);
    const avr_cycle_count_t zc_cycle = (avr_cycle_count_t)(t_zc * F_CPU);

    /* a zero-crossing triac driver latches its input at the crossing */
    for (int i = 0; i < scn.no_of_loads; ++i)
    {
      load_t *l = &scn.loads[i];
      if (l->phase != k)
        continue;
      const int conducting = load_state_at(l, zc_cycle);
      if (conducting != l->conducting)
      {
        /* m even: the voltage of phase k rises through zero */
        const double latency = t_zc - (double)l->changed_at / F_CPU;
        ++l->transitions;
        l->at_positive_zc += (m % 2) == 0;
        l->latency_sum += latency;
        if (latency > l->latency_max)
          l->latency_max = latency;
      }
      l->conducting = conducting;
    }

    /* one trace line per mains cycle, at L1's positive crossing */
    if (k == 0 && (m % 2) == 0 && trace_file)
    {
      unsigned mask = 0;
      for (int i = 0; i < scn.no_of_loads; ++i)
        mask |= (unsigned)scn.loads[i].conducting << i;
      fprintf(trace_file, "%.4f", t_zc);
      for (int j = 0; j < NO_OF_PHASES; ++j)
        fprintf(trace_file, ",%.1f", surplus_at(j, t_zc));
      for (int j = 0; j < NO_OF_PHASES; ++j)
        fprintf(trace_file, ",%.1f", -grid.export_w[j]);
      fprintf(trace_file, ",%u\n", mask);
    }
  }

  /* grid export of each phase: surplus minus the conducting loads */
  for (int k = 0; k < NO_OF_PHASES; ++k)
    grid.export_w[k] = surplus_at(k, t);
  for (int i = 0; i < scn.no_of_loads; ++i)
    if (scn.loads[i].conducting)
      grid.export_w[scn.loads[i].phase] -= scn.loads[i].watts;
}

/* ------------------------------------------------------------------------- */
/* simavr callbacks                                                            */

/* called by the ADC just before it samples: provide the input voltage */
static void adc_trigger_hook(struct avr_irq_t *irq, uint32_t value, void *param)
{
  (void)irq;
  (void)param;

  union
  {
    avr_adc_mux_t mux;
    uint32_t v;
  } e = { .v = value };

  if (adc.conversions++)
  {
    const avr_cycle_count_t period = avr->cycle - adc.last_trigger;
    if (period < adc.min_period)
      adc.min_period = period;
    if (period > adc.max_period)
      adc.max_period = period;
  }
  adc.last_trigger = avr->cycle;

  if (e.mux.kind != ADC_MUX_SINGLE || e.mux.src >= 2 * NO_OF_PHASES)
    return;

  update_grid();

  /* 3-phase PCB: V1 I1 V2 I2 V3 I3 on ADC0 to ADC5 */
  const int phase = e.mux.src / 2;
  const int is_current = e.mux.src & 1;
  const double angle = 2.0 * M_PI * (scn.frequency * now() - phase / 3.0);

  const double vrms_counts = scn.vrms / scn.vcal[phase];
  double counts;
  if (!is_current)
    counts = M_SQRT2 * vrms_counts * sin(angle);
  else
  {
    /* firmware: P = pcal x Vrms_counts x Irms_counts, positive when exporting */
    const double irms_counts = grid.export_w[phase] / (scn.pcal[phase] * vrms_counts);
    counts = M_SQRT2 * irms_counts * sin(angle);
  }

  if (counts > ADC_MAX_SWING || counts < -ADC_MAX_SWING)
  {
    counts = (counts > 0) ? ADC_MAX_SWING : -ADC_MAX_SWING;
    if (!adc.clipped++)
      fprintf(stderr, "grid_sim: warning: ADC%d clipped at %.3f s\n", e.mux.src, now());
  }

  const double millivolts = (ADC_MID_COUNTS + counts + 0.5) * 5000.0 / 1023.0;
  avr_raise_irq(avr_io_getirq(avr, AVR_IOCTL_ADC_GETIRQ, e.mux.src), (uint32_t)lround(millivolts));
}

static void isr_hook(struct avr_irq_t *irq, uint32_t value, void *param)
{
  (void)irq;
  (void)param;

  if (value)
  {
    isr.started = avr->cycle;
    return;
  }
  const avr_cycle_count_t duration = avr->cycle - isr.started;
  ++isr.count;
  isr.total += duration;
  if (duration > ADC_CYCLES_PER_CONVERSION)
    ++isr.overruns;
  if (duration > isr.max)
  {
    isr.max = duration;
    isr.max_at = now();
  }
}

static void pin_hook(struct avr_irq_t *irq, uint32_t value, void *param)
{
  (void)irq;
  load_t *l = param;
  value = !!value;
  if ((int)value == l->state)
    return;

  update_grid();

  l->prev_state = l->state;
  l->state = (int)value;
  l->changed_at = avr->cycle;
  if (value)
  {
    ++l->switches;
    if (l->first_on < 0)
      l->first_on = now();
  }
  if (events_file)
    fprintf(events_file, "%.6f,D%d,%s\n", now(), l->pin, value ? "ON" : "OFF");
}

static void serial_line(const char *line)
{
  const char *p = strstr(line, "minSampleSets/MC ");
  if (p)
  {
    const int sets = atoi(p + strlen("minSampleSets/MC "));
    ++serial.datalog_lines;
    if (sets < serial.min_sets)
      serial.min_sets = sets;
    if (sets > serial.max_sets)
      serial.max_sets = sets;
  }
  if (!quiet)
    printf("[%8.3f s] %s\n", now(), line);
}

static void uart_hook(struct avr_irq_t *irq, uint32_t value, void *param)
{
  (void)irq;
  (void)param;
  const char c = (char)value;
  if (c == '\r')
    return;
  if (c == '\n' || serial.len == sizeof serial.line - 1)
  {
    serial.line[serial.len] = '\0';
    serial_line(serial.line);
    serial.len = 0;
    if (c == '\n')
      return;
  }
  serial.line[serial.len++] = c;
}

/* ------------------------------------------------------------------------- */

static char port_of(int pin)
{
  return (pin < 8) ? 'D' : 'B';
}

static int bit_of(int pin)
{
  return (pin < 8) ? pin : pin - 8;
}

static void check_timed_expects(void)
{
  const double t = now();
  for (int i = 0; i < scn.no_of_expects; ++i)
  {
    expect_t *e = &scn.expects[i];
    if (e->done || e->kind != EXPECT_PIN || t < e->t0)
      continue;
    e->done = 1;
    int state = -1;
    for (int j = 0; j < scn.no_of_loads; ++j)
      if (scn.loads[j].pin == e->pin)
        state = scn.loads[j].state;
    e->measured = state;
    e->passed = (state == e->state);
  }
}

static int report_expects(void)
{
  int failed = 0;
  if (!scn.no_of_expects)
    return 0;

  printf("\nExpectations\n");
  for (int i = 0; i < scn.no_of_expects; ++i)
  {
    expect_t *e = &scn.expects[i];
    char what[128];
    switch (e->kind)
    {
      case EXPECT_PIN:
        snprintf(what, sizeof what, "at %.1f s, D%d %s", e->t0, e->pin, e->state ? "ON" : "OFF");
        if (!e->done)
          e->passed = 0, snprintf(what + strlen(what), sizeof what - strlen(what), "  (not reached)");
        break;
      case EXPECT_GRID_AVG:
        e->done = e->t1 <= now();
        e->measured = e->integral / (e->t1 - e->t0);
        e->passed = e->done && e->measured >= e->lo && e->measured <= e->hi;
        snprintf(what, sizeof what, "%.1f-%.1f s, grid average in [%.0f, %.0f] W: %.1f W", e->t0, e->t1, e->lo, e->hi,
                 e->measured);
        break;
      case EXPECT_ISR_MAX:
        e->passed = isr.count && isr.max <= e->hi;
        snprintf(what, sizeof what, "ADC ISR at most %.0f cycles: %llu", e->hi, (unsigned long long)isr.max);
        break;
      case EXPECT_OVERRUNS:
        e->passed = isr.count && isr.overruns <= e->hi;
        snprintf(what, sizeof what, "ADC ISR overruns at most %.0f: %llu", e->hi, (unsigned long long)isr.overruns);
        break;
      case EXPECT_MIN_SETS:
        e->passed = serial.datalog_lines && serial.min_sets >= e->lo;
        snprintf(what, sizeof what, "min sample sets per mains cycle at least %.0f: %d", e->lo,
                 serial.datalog_lines ? serial.min_sets : -1);
        break;
    }
    printf("  %s  line %3d: %s\n", e->passed ? "PASS" : "FAIL", e->line, what);
    failed += !e->passed;
  }
  printf("  %d/%d passed\n", scn.no_of_expects - failed, scn.no_of_expects);
  return failed;
}

static void usage(void)
{
  fprintf(stderr, "usage: grid_sim [-q] [-t trace.csv] [-e events.csv] firmware.elf scenario.scn\n");
  exit(2);
}

int main(int argc, char *argv[])
{
  const char *trace_path = NULL, *events_path = NULL;
  int argi = 1;
  for (; argi < argc && argv[argi][0] == '-'; ++argi)
  {
    if (!strcmp(argv[argi], "-q"))
      quiet = 1;
    else if (!strcmp(argv[argi], "-t") && argi + 1 < argc)
      trace_path = argv[++argi];
    else if (!strcmp(argv[argi], "-e") && argi + 1 < argc)
      events_path = argv[++argi];
    else
      usage();
  }
  if (argc - argi != 2)
    usage();

  const char *elf_path = argv[argi];
  snprintf(scn.name, sizeof scn.name, "%s", argv[argi + 1]);
  parse_scenario(scn.name, 0);

  elf_firmware_t fw = { 0 };
  if (elf_read_firmware(elf_path, &fw))
    die("cannot read firmware '%s'", elf_path);
  fw.frequency = F_CPU;
  fw.vcc = fw.avcc = fw.aref = 5000;

  avr = avr_make_mcu_by_name("atmega328p");
  if (!avr)
    die("atmega328p not supported by this simavr");
  avr_init(avr);
  avr->log = LOG_WARNING;
  avr_load_firmware(avr, &fw);

  /* serial output: captured here instead of simavr's console dump */
  uint32_t flags = 0;
  avr_ioctl(avr, AVR_IOCTL_UART_GET_FLAGS('0'), &flags);
  flags &= ~AVR_UART_FLAG_STDIO;
  avr_ioctl(avr, AVR_IOCTL_UART_SET_FLAGS('0'), &flags);
  avr_irq_register_notify(avr_io_getirq(avr, AVR_IOCTL_UART_GETIRQ('0'), UART_IRQ_OUTPUT), uart_hook, NULL);

  avr_irq_register_notify(avr_io_getirq(avr, AVR_IOCTL_ADC_GETIRQ, ADC_IRQ_OUT_TRIGGER), adc_trigger_hook, NULL);
  avr_irq_register_notify(avr_get_interrupt_irq(avr, ADC_VECTOR) + AVR_INT_IRQ_RUNNING, isr_hook, NULL);

  /* unconnected inputs read HIGH, like the firmware's pull-ups; then the scenario's levels */
  uint8_t ext[2] = { 0xff, 0xff }; /* PORTD, PORTB */
  for (int i = 0; i < scn.no_of_inputs; ++i)
  {
    uint8_t *e = &ext[port_of(scn.inputs[i].pin) == 'B'];
    const uint8_t mask = (uint8_t)(1U << bit_of(scn.inputs[i].pin));
    *e = scn.inputs[i].value ? (*e | mask) : (*e & ~mask);
  }
  avr_ioport_external_t ext_d = { .name = 'D', .mask = 0xff, .value = ext[0] };
  avr_ioport_external_t ext_b = { .name = 'B', .mask = 0xff, .value = ext[1] };
  avr_ioctl(avr, AVR_IOCTL_IOPORT_SET_EXTERNAL('D'), &ext_d);
  avr_ioctl(avr, AVR_IOCTL_IOPORT_SET_EXTERNAL('B'), &ext_b);

  for (int i = 0; i < scn.no_of_loads; ++i)
  {
    load_t *l = &scn.loads[i];
    avr_irq_register_notify(avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ(port_of(l->pin)), bit_of(l->pin)), pin_hook, l);
  }

  if (trace_path)
  {
    if (!(trace_file = fopen(trace_path, "w")))
      die("cannot write '%s'", trace_path);
    fprintf(trace_file, "t,surplus1,surplus2,surplus3,grid1,grid2,grid3,loads\n");
  }
  if (events_path)
  {
    if (!(events_file = fopen(events_path, "w")))
      die("cannot write '%s'", events_path);
    fprintf(events_file, "t,pin,state\n");
  }
  for (int k = 0; k < NO_OF_PHASES; ++k)
    grid.half_cycle[k] = LONG_MIN;

  const avr_cycle_count_t end = (avr_cycle_count_t)(scn.duration * F_CPU);
  int state = cpu_Running;
  while (avr->cycle < end)
  {
    state = avr_run(avr);
    if (state == cpu_Done || state == cpu_Crashed)
      break;
    check_timed_expects();
  }
  update_grid();

  /* ----------------------------------------------------------------------- */
  printf("\n== grid_sim: %s, %.1f s simulated\n", scn.name, now());
  if (state == cpu_Crashed)
    printf("  FIRMWARE CRASHED at %.3f s\n", now());

  printf("ADC       conversion period %llu-%llu cycles (expected %d), %llu conversions%s\n",
         (unsigned long long)adc.min_period, (unsigned long long)adc.max_period, ADC_CYCLES_PER_CONVERSION,
         (unsigned long long)adc.conversions, adc.clipped ? ", CLIPPED" : "");
  if (isr.count)
    printf("ADC ISR   average %.1f cycles, max %llu cycles (%.1f us) at %.3f s, %llu of %llu calls over %d cycles\n",
           (double)isr.total / isr.count, (unsigned long long)isr.max, isr.max * 1e6 / F_CPU, isr.max_at,
           (unsigned long long)isr.overruns, (unsigned long long)isr.count, ADC_CYCLES_PER_CONVERSION);
  if (serial.datalog_lines)
    printf("Datalog   %d lines, min sample sets per mains cycle %d-%d\n", serial.datalog_lines, serial.min_sets,
           serial.max_sets);
  for (int i = 0; i < scn.no_of_loads; ++i)
  {
    const load_t *l = &scn.loads[i];
    printf("Load D%-2d  L%d %5.0f W: %4u switch-ons, on %5.1f %%, %7.2f Wh, first on at %s", l->pin, l->phase + 1,
           l->watts, l->switches, 100.0 * l->on_time / now(), l->energy_wh, l->first_on < 0 ? "never" : "");
    if (l->first_on >= 0)
      printf("%.3f s", l->first_on);
    printf("\n");
    if (l->transitions)
      printf("          switching latency average %.2f ms, max %.2f ms, %u of %u at a rising zero crossing\n",
             1e3 * l->latency_sum / l->transitions, 1e3 * l->latency_max, l->at_positive_zc, l->transitions);
  }
  printf("Grid      import %.2f Wh, export %.2f Wh (L1 %.2f/%.2f, L2 %.2f/%.2f, L3 %.2f/%.2f)\n", grid.import_wh,
         grid.export_wh, grid.phase_import_wh[0], grid.phase_export_wh[0], grid.phase_import_wh[1],
         grid.phase_export_wh[1], grid.phase_import_wh[2], grid.phase_export_wh[2]);

  int failed = report_expects();

  /* guard against a simavr whose ADC runs at the wrong speed (see avr_adc.c) */
  if (adc.conversions > 1
      && (adc.min_period < ADC_CYCLES_PER_CONVERSION || adc.max_period > ADC_CYCLES_PER_CONVERSION + 16))
  {
    printf("\nERROR: ADC conversion period %llu-%llu cycles instead of %d - is the vendored avr_adc.o linked?\n",
           (unsigned long long)adc.min_period, (unsigned long long)adc.max_period, ADC_CYCLES_PER_CONVERSION);
    ++failed;
  }

  if (trace_file)
    fclose(trace_file);
  if (events_file)
    fclose(events_file);

  return (state == cpu_Crashed || failed) ? 1 : 0;
}
