# grid_sim - runs the PVRouter firmware on a simulated 3-phase grid (Linux / WSL)
#
#   make                build grid_sim
#   make check          run every scenario against the firmware, fail on any expectation
#   make check-rf       run scenarios/rf against the remote-loads firmware, with the
#                       RemoteLoadReceiver firmware on each remote unit (RX_NODES)
#   make check-rf RX_NODES=    the same with ideal receivers
#   make check-relays   run scenarios/relays against the relay firmware (configs/relays.patch)
#   make run SCN=scenarios/surplus_step.scn    run one scenario, serial output shown
#
# Every scenario is a target of its own (check-<name>, check-rf-<name>, check-relays-<name>): with -j$(nproc),
# they run in parallel, and 'make -j$(nproc) check check-rf' runs both sets together.
#
# The firmware is built separately: (cd ../Mk2_3phase_RFdatalog_temp && pio run -e basic)
# The remote-loads firmware is the rf environment with configs/remote_loads.patch applied.
# The receiver firmware is built by build_receivers.sh (needs pio), once per node ID.

SIMAVR ?= $(HOME)/.platformio/packages/tool-simavr
FW     ?= ../Mk2_3phase_RFdatalog_temp/.pio/build/basic/firmware.elf
FW_RF  ?= ../Mk2_3phase_RFdatalog_temp/.pio/build/rf/firmware.elf
# built in a build directory of its own, so that it does not replace the basic firmware
FW_RELAYS ?= ../Mk2_3phase_RFdatalog_temp/.pio/build-relays/basic/firmware.elf
SCN    ?= scenarios/surplus_step.scn
OUT    ?= out
# remote units running the receiver firmware in scenarios/rf (node IDs of configs/remote_loads.patch)
RX_NODES ?= 15 16
RX_DIR   ?= $(OUT)/receivers
RX_ARGS   = $(foreach n,$(RX_NODES),-r $(n)=$(RX_DIR)/receiver-$(n).elf)

CFLAGS  ?= -O2 -g
CFLAGS  += -std=gnu11 -I$(SIMAVR)/include/simavr
# the vendored avr_adc.o comes first: libsimavr.a's own ADC is then never linked in.
# -l:libelf.so.1 only needs the libelf runtime, not the -dev package.
LDLIBS  = $(SIMAVR)/lib/libsimavr.a -l:libelf.so.1 -lm

SCENARIOS    := $(filter-out scenarios/common.scn,$(wildcard scenarios/*.scn))
RF_SCENARIOS := $(filter-out scenarios/rf/common.scn,$(wildcard scenarios/rf/*.scn))
RELAY_SCENARIOS := $(filter-out scenarios/relays/common.scn,$(wildcard scenarios/relays/*.scn))

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

$(FW_RELAYS):
	$(error firmware not found: $(FW_RELAYS) - see configs/relays.patch)

run: grid_sim $(FW)
	@mkdir -p $(OUT)
	./grid_sim -t $(OUT)/$(notdir $(SCN:.scn=))-trace.csv -e $(OUT)/$(notdir $(SCN:.scn=))-events.csv $(FW) $(SCN)

CHECK    := $(SCENARIOS:scenarios/%.scn=check-%)
CHECK_RF := $(RF_SCENARIOS:scenarios/rf/%.scn=check-rf-%)
CHECK_RELAYS := $(RELAY_SCENARIOS:scenarios/relays/%.scn=check-relays-%)

check: $(CHECK)

check-rf: $(CHECK_RF)

check-relays: $(CHECK_RELAYS)

# the summary is shown once the scenario is over: parallel runs don't mix their lines
# $(call scenario,<firmware and receivers>,<scenario file>,<output name>)
define scenario
	@mkdir -p $(OUT)
	@./grid_sim -q -t $(OUT)/$(3)-trace.csv -e $(OUT)/$(3)-events.csv $(1) $(2) > $(OUT)/$(3)-summary.txt; \
	  status=$$?; cat $(OUT)/$(3)-summary.txt; exit $$status
endef

$(CHECK): check-%: grid_sim $(FW)
	$(call scenario,$(FW),scenarios/$*.scn,$*)

$(CHECK_RF): check-rf-%: grid_sim $(FW_RF) $(if $(RX_NODES),receivers)
	$(call scenario,$(RX_ARGS) $(FW_RF),scenarios/rf/$*.scn,rf-$*)

$(CHECK_RELAYS): check-relays-%: grid_sim $(FW_RELAYS)
	$(call scenario,$(FW_RELAYS),scenarios/relays/$*.scn,relays-$*)

receivers:
	bash build_receivers.sh $(RX_DIR) $(RX_NODES)

clean:
	rm -rf grid_sim *.o $(OUT)

.PHONY: run check check-rf check-relays $(CHECK) $(CHECK_RF) $(CHECK_RELAYS) receivers clean
