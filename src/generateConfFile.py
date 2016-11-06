import datetime
print("============================================================")
print("Script for generating a configuration file for the Dice Game")
print("------------------------------------------------------------")
print("How to use it?")
print("1. User ID")
print("     - Input a single word for the user name")
print("2. Rotations")
print("     - The program will prompt you 6 options:")
print("       x, y, z, v, q")
print("     - You can either choose the principla axes (x, y, z)")
print("       or you can input a direction vector by choosing (v)")
print("     - When option (v) is chosen you have to input 3 real")
print("       numbers which are seperated by white space or comma")
print("     - After defining the rotation axes, the program will")
print("       ask the value of the rotation angle")
print("     - If you choose option (r), the rotation axis and angle")
print("       will be coded as RANDOM")
print("     - Press (q) for ceasing rotation input")
print("     - You can input as many rotations as you wish")
print("============================================================")

participantID = input("Input the participant ID: ")

rotationList = []
flagInputRotation = True
while flagInputRotation:
    choice = input("Choose an option to input (x, y, z, v, r, q): ")
    if choice == "x":
        angle = input("Input the rotation angle: ")
        rotationList.append("1 0 0 " + angle)
    elif choice == "y":
        angle = input("Input the rotation angle: ")
        rotationList.append("0 1 0 " + angle)
    elif choice == "z":
        angle = input("Input the rotation angle: ")
        rotationList.append("0 0 1 " + angle)
    elif choice == "v":
        vector = input("Input the rotation axis vector: ")
        angle = input("Input the rotation angle: ")
        rotationList.append(vector + " " + angle)
    elif choice == "r":
        rotationList.append("RANDOM")
    elif choice == "q":
        flagInputRotation = False



with open("experiment.conf", "w") as f:
    f.write("# Configuration file generated using")
    f.write(" generateConfFile.py\n")
    f.write("# Date: %s\n\n\n" % datetime.datetime.now())
    f.write("# Participant ID: \n")
    f.write("ID %s\n" % participantID.replace(" ", ""))
    f.write("\n\n")
    f.write("# Rotations:\n")
    for rot in rotationList:
        if rot != "RANDOM":
            f.write("ROT " + rot + " DEG\n")
        else:
            f.write("ROT " + rot + "\n")
    f.write("\n")
