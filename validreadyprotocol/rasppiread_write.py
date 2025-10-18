import RPi.GPIO as GPIO
import time
from stockfish import Stockfish

# Pin Definitions (Global Constants)
CLK_PIN = 2
RVALID_PIN = 17
RREADY_PIN = 27
RDATA_PIN = 9
WVALID_PIN = 22
WREADY_PIN = 10
WDATA_PIN = 11
OVERWRITE_PIN = 0

# Global Game State
stockfish = [None, None]
is_computer = [False, False]
difficulty = [0, 0]
turn = 0
first_write = 1

def move_to_bits(move_str):
    if len(move_str) != 4 and len(move_str) != 5:
        raise ValueError("Move string must be 4 characters (e.g., 'e1e3') or 5 characters (e.g., 'e1e3q')")
    #convert string to lowercase
    move_str = move_str.lower()

    start_x = ord(move_str[0]) - ord('a')
    start_y = int(move_str[1]) - 1
    end_x = ord(move_str[2]) - ord('a')
    end_y = int(move_str[3]) - 1
    if len(move_str) == 5:
        promotion = move_str[4].lower()
        if promotion == 'q':
            promotion = 0
        elif promotion == 'r':
            promotion = 3
        elif promotion == 'b':
            promotion = 2
        elif promotion == 'n':
            promotion = 1
        else:
            raise ValueError("Invalid promotion piece. Use 'q', 'r', 'b', or 'n'.")
    else:
        promotion = 0
    
    # Convert to bits: in format of start_y, start_x, end_y, end_x, promotion (start_y has most significant bit)
    binary_representation = (
        (start_y & 0b111) << 11 |  # Start rank
        (start_x & 0b111) << 8 |   # Start file
        (end_y & 0b111) << 5 |     # End rank
        (end_x & 0b111) << 2 |     # End file
        (promotion & 0b111)        # Promotion
    )
    return binary_representation

def bits_to_move(bits, is_promotion=False):
    start_y = (bits >> 11) & 0b111
    start_x = (bits >> 8) & 0b111
    end_y = (bits >> 5) & 0b111
    end_x = (bits >> 2) & 0b111
    promotion = bits & 0b11
    start_x = chr(start_x + ord('a'))
    start_y = str(start_y + 1)
    end_x = chr(end_x + ord('a'))
    end_y = str(end_y + 1)
    if is_promotion:
        if promotion == 0:
            promotion = 'q'
        elif promotion == 3:        
            promotion = 'r'
        elif promotion == 2:
            promotion = 'b'
        elif promotion == 1:
            promotion = 'n'
        else:
            raise ValueError("Invalid promotion piece. Use 'q', 'r', 'b', or 'n'.")
        return start_x + start_y + end_x + end_y + promotion
    else:
        return start_x + start_y + end_x + end_y

def read_from_arduino():
  # Variables
    last_clk = GPIO.input(CLK_PIN)  # Store the last clock state
    receiving = False               # Flag to indicate if data is being received
    databank = []                   # List to store received data
    i = 0                           # Counter for received data bits

    # Set initial state of READY signal
    GPIO.output(RREADY_PIN, GPIO.HIGH)  # Ready to receive data
    valid = 0
    while True:
        # read the current data value, before clock edge.
        current_data = GPIO.input(RDATA_PIN)
        # Read current clock state
        current_clk = GPIO.input(CLK_PIN)
        # Read valid signal
        temp_valid = GPIO.input(RVALID_PIN)
        valid = temp_valid

        # Check for clock edge (rising edge: last_clk = 0, current_clk = 1)
        if last_clk == GPIO.LOW and current_clk == GPIO.HIGH:
            if valid == GPIO.HIGH:
                # Start receiving data
                #print("VALID IS HIGH")
                receiving = True
                GPIO.output(RREADY_PIN, GPIO.LOW)  # Not ready for new data
            if receiving:
                #print("Receiving Bit", i, current_data)
                databank.append(current_data)  # Store data in databank
                i += 1
                #print(current_data)

        # Check if 16 bits have been received
        if i >= 16:
            GPIO.output(RREADY_PIN, GPIO.LOW)
            GPIO.output(WVALID_PIN, GPIO.LOW)
            return databank

        # Update last clock state
        last_clk = current_clk

def write_to_arduino(data_bits):
    global first_write
    i = 0  # Bit index
    last_clk_state = GPIO.input(CLK_PIN)  # Store the last clock state
    
    GPIO.output(WVALID_PIN, GPIO.HIGH)
    # Send the first bit
    GPIO.output(WDATA_PIN, data_bits[i])
    if first_write:
        first_write = 0
        first_data_bit = False
        in_writing = False
        while GPIO.input(OVERWRITE_PIN) == GPIO.LOW:
            #print("OVERWRITE LOW")
            current_clk = GPIO.input(CLK_PIN)
            if last_clk_state == GPIO.LOW and current_clk == GPIO.HIGH:
                print("OVERWRITE LOW CLOCK FLASH", first_data_bit)
                GPIO.output(WDATA_PIN, first_data_bit)
                if GPIO.input(WREADY_PIN) == GPIO.HIGH:
                    in_writing = True
                if in_writing:
                    first_data_bit = not first_data_bit
            last_clk_state = current_clk
    else:
        in_writing = False
        while True:
            # Read the current clock state
            current_clk = GPIO.input(CLK_PIN)
            
            # Check for rising edge (LOW -> HIGH transition)
            if last_clk_state == GPIO.LOW and current_clk == GPIO.HIGH:
                # Check if the Arduino is ready to receive the next bit
                if GPIO.input(WREADY_PIN) == GPIO.HIGH:
                    in_writing = True
                if in_writing:
                    i += 1  # Move to the next bit
                    if i >= len(data_bits):  # Stop if all bits are sent
                        break
                    # Send the next bit
                    GPIO.output(WDATA_PIN, data_bits[i])
            
            # Update the last clock state
            last_clk_state = current_clk
    GPIO.output(RREADY_PIN, GPIO.LOW)
    GPIO.output(WVALID_PIN, GPIO.LOW)
    print("Wrote:", data_bits)

def process_received_bits(bits):
    global turn
    if not bits:
        return None

    is_move = bits[0] == 0
    color = bits[1]
    payload = bits[2:]
    
    if is_move: #actual move
        if all(bit == 0 for bit in payload): #arduino telling pi to make the first move     
            stockfish[turn].set_position()
            best_move = stockfish[turn].get_best_move()
            turn = 1
            return move_to_bits(best_move)
        
        move_bits = int(''.join(map(str, payload)), 2)
        print("Payload:", payload)
        move_str = bits_to_move(move_bits, is_promotion=bits[1])
        print("Move string:", move_str)
        stockfish[0].make_moves_from_current_position([move_str])
        stockfish[1].make_moves_from_current_position([move_str])
        
        if is_computer[1-turn]:
            best_move = stockfish[1-turn].get_best_move()
            return move_to_bits(best_move)
        turn = 1 - turn
    else: #setup
        stockfish[color].set_position() #clearing stockfish board
        is_human = all(bit == 1 for bit in payload[:14]) #is_human is true if 
        difficulty = int(''.join(map(str, payload[:14])), 2) #takes 14 bit payload, converts into individual strings, then join concatenates, then int converts binary into int
        is_computer[color] = not is_human
        if not is_human:
            stockfish[color].update_engine_parameters({"Skill Level": difficulty})
        turn = 0
    return None

def main():
    global stockfish, turn, first_write
    global first_write
    first_write = 1
    # Initialize GPIO
    GPIO.setmode(GPIO.BCM)
    
    # Input Pins
    GPIO.setup(CLK_PIN, GPIO.IN)
    GPIO.setup(RREADY_PIN, GPIO.OUT)
    GPIO.setup(WDATA_PIN, GPIO.OUT)
    GPIO.setup(OVERWRITE_PIN, GPIO.IN)
    
    # Output Pins (Initialized to LOW)
    GPIO.setup(RVALID_PIN, GPIO.IN)
    GPIO.setup(RDATA_PIN, GPIO.IN)
    GPIO.setup(WREADY_PIN, GPIO.IN)
    GPIO.setup(WVALID_PIN, GPIO.OUT)
    GPIO.output(RREADY_PIN, GPIO.LOW)
    GPIO.output(WDATA_PIN, GPIO.LOW)
    GPIO.output(WVALID_PIN, GPIO.LOW)
    
    # Initialize Stockfish
    stockfish = [Stockfish(
        path="/home/spark/Stockfish/src/stockfish",
        depth=18,
        parameters={"Threads": 2, "Minimum Thinking Time": 30}
    ), Stockfish(
        path="/home/spark/Stockfish/src/stockfish",
        depth=18,
        parameters={"Threads": 2, "Minimum Thinking Time": 30}
    )]
    
    # Initial sequence
    initial_sequence = [1,0,1,0,1,0,1,0,1,0,1,0,1,0]
    write_to_arduino(initial_sequence)
    stockfish[0].set_position()
    stockfish[1].set_position()
    
    # Main loop
    while True:
        received_bits = read_from_arduino()
        print("Received bits:", received_bits)
        response_bits = process_received_bits(received_bits)
        
        if response_bits is not None:
            print("writing to arduino", [int(b) for b in format(response_bits, '014b')])
            write_to_arduino([int(b) for b in format(response_bits, '014b')])

if __name__ == "__main__":
    try:
        main()
    finally:
        GPIO.cleanup()