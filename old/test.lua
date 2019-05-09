pulseTotal = 0
print ("Read config file... ")
file.open("config.ini", "r")
s = file.readline()
pulseTotal=s
pulseTotal=pulseTotal+0
print(pulseTotal)

file.close()  
