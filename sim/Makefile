# grid_sim - runs the PVRouter firmware on a simulated 3-phase grid (Linux / WSL)
#
#   make                build grid_sim
#   make check          run every scenario against the firmware, fail on any expectation
#   make check-rf       run scenarios/rf against the remote-loads firmware, with the
#                       RemoteLoadReceiver firmware on each remote unit (RX_NODES)
#   make check-rf RX_NODES=    the same with ideal receivers
#   make check-relays   run scenarios/relays against the relay firmware (configs/relays.patch)
#   make check-relays60 the same at 60 Hz (configs/supply_60hz.patch too), the scenarios derived into out/scn60/relays
#   make check-new      run the scenarios of 'make check' against a new-board firmware (internal
#                       1.1 V reference), with the new board's calibration read from calibration.h
#   make check-new20    the same with 20 A CTs: about the resolution of the old board
#   make check-temp    the scenarios of 'make check' against a firmware with temperature sensing (configs/temperature.patch,
#                       old board) and two DS18B20 on D3 (configs/ds18b20.scn): the 1-Wire time slots against
#                       the ADC interrupt's deadline, and the datalog must report both temperatures
#   make check-new-temp the same on the new board
#   make check-temp60   the same at 60 Hz (configs/supply_60hz.patch too): the scenarios run with `mains 60`, derived
#                       from scenarios/ into out/scn60 (the same expectations, the sample sets per cycle scaled)
#   make check-new-temp60 the same on the new board
#   make check-combo    scenarios/combo against the heaviest build (configs/temp_rf_relays.patch: remote loads, RF datalog, relays
#                       and temperature sensing), old board, the receiver firmware on each remote unit (RX_NODES)
#   make check-new-combo / check-combo60 / check-new-combo60    the same on the new board, at 60 Hz, new board at 60 Hz
#   make compare-boards after 'make check check-new': each scenario must give the same result
#                       on both boards (grid and load energies within 1 % or 0.1 Wh)
#   make compare-boards-20    after 'make check check-new20': the same within 0.5 % or 0.05 Wh
#   make compare-ct     after 'make check-new check-new20': 20 A against 50 A CTs, within 1 % or 0.1 Wh
#   make run SCN=scenarios/surplus_step.scn    run one scenario, serial output shown
#
# Every scenario is a target of its own (check-<name>, check-new-<name>, check-rf-<name>, check-relays-<name>): with -j$(nproc),
# they run in parallel, and 'make -j$(nproc) check check-rf' runs both sets together.
#
# The firmware is built separately, as CI does (README.md, Usage):
#   export PVR_SIM_BUILD=1
#   (cd .. && sim/configs/sim_config.sh)   # human-readable output (grid_sim reads it), old board
#   (cd ../Mk2_3phase_RFdatalog_temp && pio run -e basic)
# The remote-loads firmware: configs/remote_loads.patch applied, built in .pio/build-rf (README.md).
# The receiver firmware is built by build_receivers.sh (needs pio), once per node ID.

SIMAVR ?= $(HOME)/.platformio/packages/tool-simavr
FW     ?= ../Mk2_3phase_RFdatalog_temp/.pio/build/basic/firmware.elf
FW_RF  ?= ../Mk2_3phase_RFdatalog_temp/.pio/build-rf/basic/firmware.elf
# built in a build directory of its own, so that it does not replace the basic firmware
FW_RELAYS ?= ../Mk2_3phase_RFdatalog_temp/.pio/build-relays/basic/firmware.elf
# the same at 60 Hz (configs/relays.patch and configs/supply_60hz.patch)
FW_RELAYS60 ?= ../Mk2_3phase_RFdatalog_temp/.pio/build-relays60/basic/firmware.elf
# the new board (configs/sim_config.sh NEW), in a build directory of its own too
FW_NEW ?= ../Mk2_3phase_RFdatalog_temp/.pio/build-new/basic/firmware.elf
# the new board with 20 A CTs (configs/sim_config.sh NEW 20)
FW_NEW20 ?= ../Mk2_3phase_RFdatalog_temp/.pio/build-new20/basic/firmware.elf
# the old and the new board with temperature sensing (configs/temperature.patch), two DS18B20 on D3
FW_TEMP ?= ../Mk2_3phase_RFdatalog_temp/.pio/build-temp/basic/firmware.elf
FW_NEW_TEMP ?= ../Mk2_3phase_RFdatalog_temp/.pio/build-new-temp/basic/firmware.elf
# the same at 60 Hz (configs/temperature.patch and configs/supply_60hz.patch)
FW_TEMP60 ?= ../Mk2_3phase_RFdatalog_temp/.pio/build-temp60/basic/firmware.elf
FW_NEW_TEMP60 ?= ../Mk2_3phase_RFdatalog_temp/.pio/build-new-temp60/basic/firmware.elf
# remote loads, RF datalog, relays and temperature sensing in one build (configs/temp_rf_relays.patch), the same four ways
FW_COMBO ?= ../Mk2_3phase_RFdatalog_temp/.pio/build-combo/basic/firmware.elf
FW_NEW_COMBO ?= ../Mk2_3phase_RFdatalog_temp/.pio/build-new-combo/basic/firmware.elf
FW_COMBO60 ?= ../Mk2_3phase_RFdatalog_temp/.pio/build-combo60/basic/firmware.elf
FW_NEW_COMBO60 ?= ../Mk2_3phase_RFdatalog_temp/.pio/build-new-combo60/basic/firmware.elf
CT_LOW  ?= 20
CAL    ?= ../Mk2_3phase_RFdatalog_temp/calibration.h
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
COMBO_SCENARIOS := $(filter-out scenarios/combo/common.scn,$(wildcard scenarios/combo/*.scn))

grid_sim: grid_sim.o rfm69.o ds18b20.o avr_adc.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

grid_sim.o: grid_sim.c rfm69.h ds18b20.h
	$(CC) $(CFLAGS) -Wall -Wextra -c -o $@ $<

rfm69.o: rfm69.c rfm69.h
	$(CC) $(CFLAGS) -Wall -Wextra -c -o $@ $<

ds18b20.o: ds18b20.c ds18b20.h
	$(CC) $(CFLAGS) -Wall -Wextra -c -o $@ $<

# upstream code, built as is
avr_adc.o: avr_adc.c
	$(CC) $(CFLAGS) -w -c -o $@ $<

$(FW):
	$(error firmware not found: $(FW) - build it first: export PVR_SIM_BUILD=1, run sim/configs/sim_config.sh from the repository root, then 'pio run -e basic' (README.md, Usage))

$(FW_RF):
	$(error firmware not found: $(FW_RF) - apply configs/remote_loads.patch, then 'PLATFORMIO_BUILD_DIR=.pio/build-rf pio run -e basic' (README.md))

$(FW_RELAYS60):
	$(error firmware not found: $(FW_RELAYS60) - sim_config.sh, apply configs/relays.patch and configs/supply_60hz.patch, then 'PLATFORMIO_BUILD_DIR=.pio/build-relays60 pio run -e basic' (README.md))

$(FW_RELAYS):
	$(error firmware not found: $(FW_RELAYS) - see configs/relays.patch)

$(FW_NEW):
	$(error firmware not found: $(FW_NEW) - see check-new in README.md)

$(FW_NEW20):
	$(error firmware not found: $(FW_NEW20) - see check-new20 in README.md)

$(FW_TEMP):
	$(error firmware not found: $(FW_TEMP) - sim_config.sh, apply configs/temperature.patch, then 'PLATFORMIO_BUILD_DIR=.pio/build-temp pio run -e basic' (README.md))

$(FW_NEW_TEMP):
	$(error firmware not found: $(FW_NEW_TEMP) - sim_config.sh NEW, apply configs/temperature.patch, then 'PLATFORMIO_BUILD_DIR=.pio/build-new-temp pio run -e basic' (README.md))

$(FW_TEMP60):
	$(error firmware not found: $(FW_TEMP60) - sim_config.sh, apply configs/temperature.patch and configs/supply_60hz.patch, then 'PLATFORMIO_BUILD_DIR=.pio/build-temp60 pio run -e basic' (README.md))

$(FW_NEW_TEMP60):
	$(error firmware not found: $(FW_NEW_TEMP60) - sim_config.sh NEW, apply configs/temperature.patch and configs/supply_60hz.patch, then 'PLATFORMIO_BUILD_DIR=.pio/build-new-temp60 pio run -e basic' (README.md))

run: grid_sim $(FW)
	@mkdir -p $(OUT)
	./grid_sim -t $(OUT)/$(notdir $(SCN:.scn=))-trace.csv -e $(OUT)/$(notdir $(SCN:.scn=))-events.csv $(FW) $(SCN)

CHECK    := $(SCENARIOS:scenarios/%.scn=check-%)
CHECK_RF := $(RF_SCENARIOS:scenarios/rf/%.scn=check-rf-%)
CHECK_RELAYS := $(RELAY_SCENARIOS:scenarios/relays/%.scn=check-relays-%)
CHECK_RELAYS60 := $(RELAY_SCENARIOS:scenarios/relays/%.scn=check-relays60-%)
CHECK_NEW := $(SCENARIOS:scenarios/%.scn=check-new-%)
CHECK_NEW20 := $(SCENARIOS:scenarios/%.scn=check-new20-%)
CHECK_TEMP := $(SCENARIOS:scenarios/%.scn=check-temp-%)
CHECK_NEW_TEMP := $(SCENARIOS:scenarios/%.scn=check-new-temp-%)
CHECK_TEMP60 := $(SCENARIOS:scenarios/%.scn=check-temp60-%)
CHECK_NEW_TEMP60 := $(SCENARIOS:scenarios/%.scn=check-new-temp60-%)
CHECK_COMBO := $(COMBO_SCENARIOS:scenarios/combo/%.scn=check-combo-%)
CHECK_NEW_COMBO := $(COMBO_SCENARIOS:scenarios/combo/%.scn=check-new-combo-%)
CHECK_COMBO60 := $(COMBO_SCENARIOS:scenarios/combo/%.scn=check-combo60-%)
CHECK_NEW_COMBO60 := $(COMBO_SCENARIOS:scenarios/combo/%.scn=check-new-combo60-%)
# the scenarios at 60 Hz: scenarios/ with the mains frequency changed (common.scn is among them, the others include it)
SCN60 := $(SCENARIOS:scenarios/%.scn=$(OUT)/scn60/%.scn) $(OUT)/scn60/common.scn
SCN60_COMBO := $(COMBO_SCENARIOS:scenarios/combo/%.scn=$(OUT)/scn60/combo/%.scn) $(OUT)/scn60/combo/common.scn
SCN60_RELAYS := $(RELAY_SCENARIOS:scenarios/relays/%.scn=$(OUT)/scn60/relays/%.scn) $(OUT)/scn60/relays/common.scn

check: $(CHECK)

check-rf: $(CHECK_RF)

check-relays: $(CHECK_RELAYS)

check-relays60: $(CHECK_RELAYS60)

check-new: $(CHECK_NEW)

check-new20: $(CHECK_NEW20)

check-temp: $(CHECK_TEMP)

check-new-temp: $(CHECK_NEW_TEMP)

check-temp60: $(CHECK_TEMP60)

check-new-temp60: $(CHECK_NEW_TEMP60)

check-combo: $(CHECK_COMBO)

check-new-combo: $(CHECK_NEW_COMBO)

check-combo60: $(CHECK_COMBO60)

check-new-combo60: $(CHECK_NEW_COMBO60)

# read the summaries of the last 'make check check-new' / 'make check check-new20'
compare-boards:
	sh compare_boards.sh $(OUT) new 1 0.1

compare-boards-20:
	sh compare_boards.sh $(OUT) new20 0.5 0.1

compare-ct:
	sh compare_boards.sh $(OUT) new20 1 0.1 new

# the summary is shown once the scenario is over: parallel runs don't mix their lines
# $(call scenario,<firmware and receivers>,<scenario file>,<output name>)
define scenario
	@mkdir -p $(OUT)
	@./grid_sim -q -t $(OUT)/$(3)-trace.csv -e $(OUT)/$(3)-events.csv $(1) $(2) > $(OUT)/$(3)-summary.txt; \
	  status=$$?; cat $(OUT)/$(3)-summary.txt; exit $$status
endef

$(CHECK): check-%: grid_sim $(FW)
	$(call scenario,$(FW),scenarios/$*.scn,$*)

# the same scenarios, with the new board's calibration replacing that of scenarios/common.scn
$(OUT)/new_board.scn: board_scn.sh $(CAL)
	@mkdir -p $(OUT)
	sh board_scn.sh NEW $(CAL) > $@

$(CHECK_NEW): check-new-%: grid_sim $(FW_NEW) $(OUT)/new_board.scn
	$(call scenario,-b $(OUT)/new_board.scn $(FW_NEW),scenarios/$*.scn,new-$*)

$(OUT)/new20_board.scn: board_scn.sh $(CAL)
	@mkdir -p $(OUT)
	sh board_scn.sh NEW $(CAL) $(CT_LOW) > $@

$(CHECK_NEW20): check-new20-%: grid_sim $(FW_NEW20) $(OUT)/new20_board.scn
	$(call scenario,-b $(OUT)/new20_board.scn $(FW_NEW20),scenarios/$*.scn,new20-$*)

$(CHECK_RF): check-rf-%: grid_sim $(FW_RF) $(if $(RX_NODES),receivers)
	$(call scenario,$(RX_ARGS) $(FW_RF),scenarios/rf/$*.scn,rf-$*)

$(CHECK_RELAYS): check-relays-%: grid_sim $(FW_RELAYS)
	$(call scenario,$(FW_RELAYS),scenarios/relays/$*.scn,relays-$*)

$(CHECK_RELAYS60): check-relays60-%: grid_sim $(FW_RELAYS60) $(SCN60_RELAYS)
	$(call scenario,$(FW_RELAYS60),$(OUT)/scn60/relays/$*.scn,relays60-$*)

# Temperature sensing: the two DS18B20 of configs/ds18b20.scn on D3, which must be read after their
# conversion and reported by the datalog; the board files of the old and the new board
$(OUT)/temp_board.scn: board_scn.sh $(CAL) configs/ds18b20.scn
	@mkdir -p $(OUT)
	{ sh board_scn.sh OLD $(CAL); cat configs/ds18b20.scn; } > $@

$(OUT)/new_temp_board.scn: board_scn.sh $(CAL) configs/ds18b20.scn
	@mkdir -p $(OUT)
	{ sh board_scn.sh NEW $(CAL); cat configs/ds18b20.scn; } > $@

$(CHECK_TEMP): check-temp-%: grid_sim $(FW_TEMP) $(OUT)/temp_board.scn
	$(call scenario,-b $(OUT)/temp_board.scn $(FW_TEMP),scenarios/$*.scn,temp-$*)

$(CHECK_NEW_TEMP): check-new-temp-%: grid_sim $(FW_NEW_TEMP) $(OUT)/new_temp_board.scn
	$(call scenario,-b $(OUT)/new_temp_board.scn $(FW_NEW_TEMP),scenarios/$*.scn,new_temp-$*)

# 60 Hz: 50 Hz of the scenarios becomes 60, and the sample sets per mains cycle (26.7 instead of 32) scale with it
$(OUT)/scn60/%.scn: scenarios/%.scn
	@mkdir -p $(@D)
	awk '/^mains 50 / { sub(/^mains 50 /, "mains 60 ") } /^expect min_sample_sets / { $$3 = int($$3 * 50 / 60) } { print }' $< > $@

$(CHECK_TEMP60): check-temp60-%: grid_sim $(FW_TEMP60) $(OUT)/temp_board.scn $(SCN60)
	$(call scenario,-b $(OUT)/temp_board.scn $(FW_TEMP60),$(OUT)/scn60/$*.scn,temp60-$*)

$(CHECK_NEW_TEMP60): check-new-temp60-%: grid_sim $(FW_NEW_TEMP60) $(OUT)/new_temp_board.scn $(SCN60)
	$(call scenario,-b $(OUT)/new_temp_board.scn $(FW_NEW_TEMP60),$(OUT)/scn60/$*.scn,new_temp60-$*)

$(FW_COMBO):
	$(error firmware not found: $(FW_COMBO) - sim_config.sh, apply configs/temp_rf_relays.patch, then 'PLATFORMIO_BUILD_DIR=.pio/build-combo pio run -e basic' (README.md))

$(FW_NEW_COMBO):
	$(error firmware not found: $(FW_NEW_COMBO) - sim_config.sh NEW, apply configs/temp_rf_relays.patch, then 'PLATFORMIO_BUILD_DIR=.pio/build-new-combo pio run -e basic' (README.md))

$(FW_COMBO60):
	$(error firmware not found: $(FW_COMBO60) - sim_config.sh, apply configs/temp_rf_relays.patch and configs/supply_60hz.patch, then 'PLATFORMIO_BUILD_DIR=.pio/build-combo60 pio run -e basic' (README.md))

$(FW_NEW_COMBO60):
	$(error firmware not found: $(FW_NEW_COMBO60) - sim_config.sh NEW, apply configs/temp_rf_relays.patch and configs/supply_60hz.patch, then 'PLATFORMIO_BUILD_DIR=.pio/build-new-combo60 pio run -e basic' (README.md))

# the combination: the board files of the temperature targets, the RF receivers of check-rf; 60 Hz as check-temp60
$(CHECK_COMBO): check-combo-%: grid_sim $(FW_COMBO) $(OUT)/temp_board.scn $(if $(RX_NODES),receivers)
	$(call scenario,-b $(OUT)/temp_board.scn $(RX_ARGS) $(FW_COMBO),scenarios/combo/$*.scn,combo-$*)

$(CHECK_NEW_COMBO): check-new-combo-%: grid_sim $(FW_NEW_COMBO) $(OUT)/new_temp_board.scn $(if $(RX_NODES),receivers)
	$(call scenario,-b $(OUT)/new_temp_board.scn $(RX_ARGS) $(FW_NEW_COMBO),scenarios/combo/$*.scn,new_combo-$*)

$(CHECK_COMBO60): check-combo60-%: grid_sim $(FW_COMBO60) $(OUT)/temp_board.scn $(SCN60_COMBO) $(if $(RX_NODES),receivers)
	$(call scenario,-b $(OUT)/temp_board.scn $(RX_ARGS) $(FW_COMBO60),$(OUT)/scn60/combo/$*.scn,combo60-$*)

$(CHECK_NEW_COMBO60): check-new-combo60-%: grid_sim $(FW_NEW_COMBO60) $(OUT)/new_temp_board.scn $(SCN60_COMBO) $(if $(RX_NODES),receivers)
	$(call scenario,-b $(OUT)/new_temp_board.scn $(RX_ARGS) $(FW_NEW_COMBO60),$(OUT)/scn60/combo/$*.scn,new_combo60-$*)

receivers:
	bash build_receivers.sh $(RX_DIR) $(RX_NODES)

clean:
	rm -rf grid_sim *.o $(OUT)

.PHONY: run check check-new check-new20 check-temp check-new-temp check-temp60 check-new-temp60 check-combo check-new-combo check-combo60 check-new-combo60 compare-boards compare-boards-20 compare-ct check-rf check-relays check-relays60 $(CHECK) $(CHECK_NEW) $(CHECK_NEW20) $(CHECK_TEMP) $(CHECK_NEW_TEMP) $(CHECK_TEMP60) $(CHECK_NEW_TEMP60) $(CHECK_COMBO) $(CHECK_NEW_COMBO) $(CHECK_COMBO60) $(CHECK_NEW_COMBO60) $(CHECK_RF) $(CHECK_RELAYS) $(CHECK_RELAYS60) receivers clean
