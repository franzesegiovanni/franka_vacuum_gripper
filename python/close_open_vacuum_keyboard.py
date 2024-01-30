from franka_vacuum_gripper.msg import VacuumActionGoal, DropOffActionGoal
import rospy
from pynput.keyboard import Listener, KeyCode, Key

class Vacuum_Gripper():
    def __init__(self):
        rospy.init_node("vacuum_gripper")
        self.r=rospy.Rate(20)
        self.end=False

        self.vacuum_pub = rospy.Publisher("/franka_vacuum_gripper/vacuum/goal", VacuumActionGoal,
                                           queue_size=0)
        self.dropoff_pub = rospy.Publisher("/franka_vacuum_gripper/dropoff/goal", DropOffActionGoal,
                                           queue_size=0)

        
        self.vacuum= False  
        self.dropoff=False
        self.listener = Listener(on_press=self._on_press)
        self.listener.start()

        self.vacuum_command=   VacuumActionGoal()
        self.dropoff_command = DropOffActionGoal()
        
        rospy.sleep(2)
        
    def _on_press(self, key):
        # This function runs on the background and checks if a keyboard key was pressed
        if key == Key.esc:
            self.end = True
        if key == KeyCode.from_char('d'):
            self.vacuum= False  
            self.dropoff= True
            print("Vacuum deactivated")
             
        if key == KeyCode.from_char('v'):
            self.dropoff= False
            self.vacuum= True
            print(" Vacuum active")
            
    
    def control(self):
        while not self.end:
            
            if self.vacuum:
                self.vacuum_pub.publish(self.vacuum_command)
                
                
            if self.dropoff:
                self.dropoff_pub.publish(self.dropoff_command)
                
            
            self.r.sleep()
            
            
if __name__ == "__main__":

    vacuum_gripper=Vacuum_Gripper()
    vacuum_gripper.control()