/**
   @file cal_CTx_v_meter.ino
   @author Robin Emley (www.Mk2PVrouter.co.uk)
   @author Frederic Metrich (frederic.metrich@live.fr)
   @brief cal_CTx_v_meter.ino - A calibration program for the energy diverter.
   @date 2020-05-26

   @mainpage A 3-phase calibration program for the router/diverter

   @section description Description
   cal_CTx_v_meter.ino - Arduino program used to calibrate the 3-phase energy diverter.

   @section history History
   __February 2018__
   This calibration sketch is based on Mk2_bothDisplays_4.ino. Its purpose is to
   mimic the behaviour of a digital electricity meter.

   CT1 should be clipped around one of the live cables that pass through the
   meter. The energy flow measured by CT1 is noted and a short pulse is generated
   whenever a pre-set amount of energy has been recorded (normally 3600J).

   This stream of pulses can then be compared against optical pulses from a standard
   electrical utility meter. The pulse rate can be varied by adjusting the value
   of powerCal_grid.  When the two streams of pulses are in synch, correct calibration
   of the CT1 channel has been achieved.

        Robin Emley
        www.Mk2PVrouter.co.uk

   __September 2019__
   This calibration sketch is based on cal_CT1_v_meter.ino.
   It is intended to be used with the 3-phase rev 2 PCB.
   Each CT/phase can be calibrated be setting the appropriate 'CURRENT_CAL_PHASE'.

   __May 2020__
   This sketch has been completely re-worked to calibrate all3 CTs at once.
   Every 5 seconds, a set of data is printed on the serial monitor.

   __June 2020__
   Some tiny fixes.
   Added voltage calibration for each phase separately (only for display purpose).

   @author Fred Metrich
   @copyright Copyright (c) 2020

*/
static_assert(__cplusplus >= 201703L, "**** Please define 'gnu++17' in 'platform.txt' ! ****");
static_assert(__cplusplus >= 201703L, "See also : https://github.com/FredM67/PVRouter-3-phase/blob/main/runtime/Mk2_3phase_RFdatalog_temp/Readme.md");
