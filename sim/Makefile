# grid_sim - runs the PVRouter firmware on a simulated 3-phase grid (Linux / WSL)
#
#   make                build grid_sim
#   make check          run every scenario against the firmware, fail on any expectation
#   make check-rf       run scenarios/rf against the remote-loads firmware, with the
#                       RemoteLoadReceiver firmware on each remote unit (RX_NODES)
#   make check-rf RX_NODES=    the same with ideal receivers
#   make run SCN=scenarios/surplus_step.scn    run one scenario, serial output shown
#
# The firmware is built separately: (cd ../Mk2_3phase_RFdatalog_temp && pio run -e basic)
# The remote-loads firmware is the rf environment with configs/remote_loads.patch applied.
# The receiver firmware is built by build_receivers.sh (needs pio), once per node ID.

SIMAVR ?= $(HOME)/.platformio/packages/tool-simavr
FW     ?= ../Mk2_3phase_RFdatalog_temp/.pio/build/basic/firmware.elf
FW_RF  ?= ../Mk2_3phase_RFdatalog_temp/.pio/build/rf/firmware.elf
SCN    ?= scenarios/surplus_step.scn
OUT    ?= out
# remote units running the receiver firmware in scenarios/rf (node IDs of configs/remote_loads.patch)
RX_NODES ?= 15
RX_DIR   ?= $(OUT)/receivers
RX_ARGS   = $(foreach n,$(RX_NODES),-r $(n)=$(RX_DIR)/receiver-$(n).elf)

CFLAGS  ?= -O2 -g
CFLAGS  += -std=gnu11 -I$(SIMAVR)/include/simavr
# the vendored avr_adc.o comes first: libsimavr.a's own ADC is then never linked in.
# -l:libelf.so.1 only needs the libelf runtime, not the -dev package.
LDLIBS  = $(SIMAVR)/lib/libsimavr.a -l:libelf.so.1 -lm

SCENARIOS    := $(filter-out scenarios/common.scn,$(wildcard scenarios/*.scn))
RF_SCENARIOS := $(filter-out scenarios/rf/common.scn,$(wildcard scenarios/rf/*.scn))

grid_sim: grid_sim.o rfm69.o avr_adc.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

grid_sim.o: grid_sim.c rfm69.h
	$(CC) $(CFLAGS) -Wall -Wextra -c -o $@ $<

rfm69.o: rfm69.c rfm69.h
	$(CC) $(CFLAGS) -Wall -Wextra -c -o $@ $<

# upstream code, built as is
avr_adc.o: avr_adc.c
	$(CC) $(CFLAGS) -w -c -o $@ $<

$(FW):
	$(error firmware not found: $(FW) - build it first with 'pio run -e basic')

$(FW_RF):
	$(error firmware not found: $(FW_RF) - apply configs/remote_loads.patch, then 'pio run -e rf')

# $(call scenarios,<scenario files>,<firmware>,<output prefix>)
define scenarios
	@mkdir -p $(OUT)
	@failed=0; for s in $(1); do \
	  n=$(3)$$(basename $$s .scn); \
	  ./grid_sim -q -t $(OUT)/$$n-trace.csv -e $(OUT)/$$n-events.csv $(2) $$s | tee $(OUT)/$$n-summary.txt || failed=1; \
	done; exit $$failed
endef

run: grid_sim $(FW)
	@mkdir -p $(OUT)
	./grid_sim -t $(OUT)/$(notdir $(SCN:.scn=))-trace.csv -e $(OUT)/$(notdir $(SCN:.scn=))-events.csv $(FW) $(SCN)

check: grid_sim $(FW)
	$(call scenarios,$(SCENARIOS),$(FW),)

check-rf: grid_sim $(FW_RF) $(if $(RX_NODES),receivers)
	$(call scenarios,$(RF_SCENARIOS),$(RX_ARGS) $(FW_RF),rf-)

receivers:
	bash build_receivers.sh $(RX_DIR) $(RX_NODES)

clean:
	rm -rf grid_sim *.o $(OUT)

.PHONY: run check check-rf receivers clean
