print("Wait 2 seconds please")
tmr.alarm(0, 2000, 0, function() dofile('energyMeter.lua') end)
--tmr.alarm(0, 2000, 0, function() dofile('energyMeterChata.lua') end)
