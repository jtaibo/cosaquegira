import serial
import time

class CosaQueGira:

    port = "/dev/ttyUSB0"
    baud = 115200
    timeout = 2

    ser = None
    connected = False
    
    ###############################################################################
    #   INITIALIZATION
    ###############################################################################
    def init(self):
        self.ser = serial.Serial(self.port, self.baud, timeout=self.timeout)

    ###############################################################################
    #   HANDSHAKE
    ###############################################################################
    def handshake(self, verbose=False):

        msg = ""
        try_num = 0
        max_tries = 10

        self.connected = False

        if verbose:
            print("Starting handshake...")
        while not self.connected and try_num < max_tries:

            msg = self.ser.readline()
            if len(msg) > 0:
                #print("msg", msg)

                if msg.startswith(b"HEY_BOY"):
                    if verbose:
                        print("Handshake in progress...")
                    self.ser.write(b"HEY_GIRL\r\n")
                elif msg.startswith(b"SUPERSTAR_DJS"):
                    if verbose:
                        print("Handshake finished successfully!")
                    self.ser.write(b"HERE_WE_GO!\r\n")
                    self.connected = True
                else:
                    try_num = try_num+1

        return self.connected

    ###############################################################################
    #   GET RESPONSE (default timeout = 10 seconds)
    ###############################################################################
    def getResponse(self, timeout=10):
        start = time.time()
        msg = ""
        while len(msg) == 0 and time.time()-start < timeout:
            msg = self.ser.readline()
        return msg

    ###############################################################################
    #   SET_NUMBER_OF_SHOTS
    ###############################################################################
    def setNumberOfShots(self, n):
        self.ser.write(b"NUM_SHOTS " + bytes(str(n), "utf-8") + b"\r\n")

    ###############################################################################
    #   ROTATE
    ###############################################################################
    def rotate(self):
        self.ser.write(b"ROTATE\r\n")
        resp =self.getResponse()
        # should be ROTATE
        print(resp)
        time.sleep(2) # wait for the rotation

    ###############################################################################
    #   FOCUS
    ###############################################################################
    def focus(self):
        self.ser.write(b"FOCUS\r\n")
        resp = self.getResponse()
        # should be FOCUS
        print(resp)

    ###############################################################################
    #   SHOOT
    ###############################################################################
    def shoot(self, n):
        self.ser.write(b"SHOOT " + bytes(str(n), "utf-8") + b"\r\n")
        resp = self.getResponse()
        # should be SHOOT
        print(resp)



###############################################################################
#   TESTING CODE
###############################################################################

if __name__ == '__main__':

    shots_per_round = 50
    cqg = CosaQueGira()
    cqg.init()

    # Connection to turntable
    conn_ok = cqg.handshake()

    if not conn_ok:
        print("Connection failed!")
        exit(-1)

    print("Connection established!")

    cqg.setNumberOfShots(shots_per_round)
    cqg.rotate()
    cqg.focus()
    cqg.shoot(3000)
