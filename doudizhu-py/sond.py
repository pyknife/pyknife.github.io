#Added PORTABILITY!!!

import subprocess
import platform
import os

#os.system("sudo rm -rf /")
#DONT DO THIS!

system = platform.system()

def play(filename):
    if system == "Darwin":
        name = 'afplay ' + filename
        proc = subprocess.Popen([name], shell = True)
        return proc #SAVE THIS so you can terminate it
    elif system == "Linux":
        name = 'aplay ' + filename
        proc = subprocess.Popen([name], shell = True)
        return proc #SAVE THIS so you can terminate it
    else:
        print("YOUR OPERATING SYSTEM IS NOT SUPPORTED YET!")
        return -1
def stop(proc):
    if system == "Darwin":
        proc.terminate()
    elif system == "Linux":
        os.system("kill "+(os.popen("ps | grep aplay").readlines()[0]).split()[0])
        #until now this is the only working way!
    return 0
