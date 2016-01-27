--Init  
DeviceID="esp02"  
RoomID="2"  
Energy            = 0
pulseTotal        = 0
pulseHour         = 0
pulseDay          = 0
pulseCountkWh     = 700    --pocet impulsu na 1 kWh
WhkWh             = 1000   --pocet Wh na kWh
diffForSavekWh    = 1/10    --diference kWh pro zapis do EEPROM
minuteOld         = 0
hourOld           = 0
Consumption       = 0
cycles            = 0

wifi.setmode(wifi.STATION)
wifi.sta.config("Datlovo","Nu6kMABmseYwbCoJ7LyG")

Broker="213.192.58.66"  

pinLed = 4
gpio.mode(pinLed,gpio.OUTPUT)  
gpio.write(pinLed,gpio.HIGH)  

versionSW         = "0.1"
versionSWString   = "EnergyMeter v" 
print(versionSWString .. versionSW)

sntp.sync('192.168.1.2', function(sec,usec,server)
  print('Time sync ', sec, usec, server)
end,
function()
 print('Time sync failed!')
end)

-- -- use pin 7 as the input pulse width counter
-- function pinPulse(level)
  -- if level == gpio.LOW then
    -- du = tmr.now() - pulse1
    -- print(du)
    -- gpio.write(pinLed,gpio.HIGH)  
    -- pulseTotal=pulseTotal+1
    -- pulseDay=pulseDay+1
    -- pulseHour=pulseHour+1
  -- else
    -- pulse1 = tmr.now()
    -- gpio.write(pinLed,gpio.LOW)  
  -- end
-- end


pin = 7
pulse1 = 0
du=0
gpio.mode(pin,gpio.INT)
function pinPulse(level)
  du = tmr.now() - pulse1
  print(du)
  pulse1 = tmr.now()
  if level == gpio.HIGH then 
    gpio.trig(pin, "down") 
    gpio.write(pinLed,gpio.LOW)  
  else 
    gpio.trig(pin, "up") 
    pulseTotal=pulseTotal+1
    pulseDay=pulseDay+1
    pulseHour=pulseHour+1  
    consumption = consumption + 3600000/(tmr.now()/1000 - lastPulse)
    cycles=cycles+1
    lastPulse=tmr.now()    
    gpio.write(pinLed,gpio.HIGH)  
  end
end
gpio.trig(pin, "down", pinPulse)

tmr.alarm(2, 10000, 1, function() 
  print("I am sending data to OpenHab")
  Energy = pulse2kWh(pulseTotal)
  print(Energy)
  if cycles>0 then
    m:publish("/home/".. RoomID .."/" .. DeviceID .. "/p1/Consumption",Consumption/cycles,0,0)  
    cycles=0
    Consumption=0
  end
  m:publish("/home/".. RoomID .."/" .. DeviceID .. "/p1/Energy",Energy,0,0)  
 -- if (minute() < minuteOld) {
    -- minuteOld=0;
    -- pulseHour=0;
  -- }
  -- if (hour() < hourOld) {
    -- hourOld=0;
    -- pulseDay=0;
  -- }
end)

-- tmr.alarm(3, 100, 1, function() 
  -- pulseTotal=pulseTotal+1
-- end)

m = mqtt.Client("ESP8266".. DeviceID, 180, "datel", "hanka")  
m:lwt("/lwt", "ESP8266", 0, 0)  
m:on("offline", function(con)   
  print ("Mqtt Reconnecting....")   
  tmr.alarm(1, 10000, 0, function()  
    m:connect(Broker, 31883, 0, function(conn)   
      print("Mqtt Connected to:" .. Broker)  
      mqtt_sub() --run the subscription function  
    end)  
  end)  
end)  

function mqtt_sub()  
  m:subscribe("/home/".. RoomID .."/" .. DeviceID .. "/p1/Energy",0, function(conn)   
    print("Mqtt Subscribed to OpenHAB feed for device " .. DeviceID .. "(EnergyMeter)")  
  end)  
end  

 -- on publish message receive event  
m:on("message", function(conn, topic, data)   
  print("Received:" .. topic .. ":" .. data) 
end)  

tmr.alarm(0, 1000, 1, function()  
  if wifi.sta.status() == 5 and wifi.sta.getip() ~= nil then  
    tmr.stop(0)  
    m:connect(Broker, 31883, 0, function(conn)   
      print("Mqtt Connected to:" .. Broker)  
      mqtt_sub() --run the subscription function  
--      tmr.alarm(1, 5000, 1, function() 
--      end)
    end)
  end
end)

function pulse2kWh(pulse) 
  return pulse/pulseCountkWh
end

function kWh2Pulse(kWh)
  return kWh*pulseCountkWh
end

function Wh2kWh(Wh) 
  return Wh/WhkWh
end
