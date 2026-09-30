# grid_sim - runs the PVRouter firmware on a simulated 3-phase grid (Linux / WSL)
#
#   make                build grid_sim
#   make check          run every scenario against the firmware, fail on any expectation
#   make run SCN=scenarios/surplus_step.scn    run one scenario, serial output shown
#
# The firmware is built separately: (cd ../Mk2_3phase_RFdatalog_temp && pio run -e basic)

SIMAVR ?= $(HOME)/.platformio/packages/tool-simavr
FW     ?= ../Mk2_3phase_RFdatalog_temp/.pio/build/basic/firmware.elf
SCN    ?= scenarios/surplus_step.scn
OUT    ?= out

CFLAGS  ?= -O2 -g
CFLAGS  += -std=gnu11 -I$(SIMAVR)/include/simavr
# the vendored avr_adc.o comes first: libsimavr.a's own ADC is then never linked in.
# -l:libelf.so.1 only needs the libelf runtime, not the -dev package.
LDLIBS  = $(SIMAVR)/lib/libsimavr.a -l:libelf.so.1 -lm

SCENARIOS := $(filter-out scenarios/common.scn,$(wildcard scenarios/*.scn))

grid_sim: grid_sim.o avr_adc.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

grid_sim.o: grid_sim.c
	$(CC) $(CFLAGS) -Wall -Wextra -c -o $@ $<

# upstream code, built as is
avr_adc.o: avr_adc.c
	$(CC) $(CFLAGS) -w -c -o $@ $<

$(FW):
	$(error firmware not found: $(FW) - build it first with 'pio run -e basic')

run: grid_sim $(FW)
	@mkdir -p $(OUT)
	./grid_sim -t $(OUT)/$(notdir $(SCN:.scn=))-trace.csv -e $(OUT)/$(notdir $(SCN:.scn=))-events.csv $(FW) $(SCN)

check: grid_sim $(FW)
	@mkdir -p $(OUT)
	@failed=0; for s in $(SCENARIOS); do \
	  n=$$(basename $$s .scn); \
	  ./grid_sim -q -t $(OUT)/$$n-trace.csv -e $(OUT)/$$n-events.csv $(FW) $$s | tee $(OUT)/$$n-summary.txt || failed=1; \
	done; exit $$failed

clean:
	rm -rf grid_sim *.o $(OUT)

.PHONY: run check clean
