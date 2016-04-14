print("Wait 5 seconds please")
tmr.alarm(0, 5000, 0, function() dofile('energyMeter.lua') end)
--tmr.alarm(0, 10000, 0, function() dofile('energyMeterChata.lua') end)
