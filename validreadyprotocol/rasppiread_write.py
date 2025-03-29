import RPi.GPIO as GPIO
import time
from stockfish import Stockfish

# Pin Definitions (Global Constants)
CLK_PIN = 4
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

def move_to_bits(move_str):
    if len(move_str) != 4:
        raise ValueError("Move string must be 4 characters (e.g., 'e1e3')")
    
    start_file = ord(move_str[0].lower()) - ord('a')
    start_rank = int(move_str[1]) - 1
    end_file = ord(move_str[2].lower()) - ord('a')
    end_rank = int(move_str[3]) - 1

    return (start_file << 11) | (start_rank << 8) | (end_file << 5) | (end_rank << 2)

def bits_to_move(bits):
    start_file = (bits >> 11) & 0b111
    start_rank = (bits >> 8) & 0b111
    end_file = (bits >> 5) & 0b111
    end_rank = (bits >> 2) & 0b111
    return f"{chr(start_file + ord('a'))}{start_rank + 1}{chr(end_file + ord('a'))}{end_rank + 1}"

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
                receiving = True
                GPIO.output(RREADY_PIN, GPIO.LOW)  # Not ready for new data
            if receiving:
                databank.append(current_data)  # Store data in databank
                i += 1

        # Check if 16 bits have been received
        if i >= 16:
            GPIO.output(RREADY_PIN, GPIO.LOW)
            GPIO.output(WVALID_PIN, GPIO.LOW)
            return databank

        # Update last clock state
        last_clk = current_clk

def write_to_arduino(data_bits):
    i = 0  # Bit index
    last_clk_state = GPIO.input(CLK_PIN)  # Store the last clock state
    
    # Send the first bit
    GPIO.output(RDATA_PIN, data_bits[i])
    if first_write:
        first_write = 0
        while OVERWRITE_PIN == GPIO.LOW:
            current_clk = GPIO.input(CLK_PIN)
            if last_clk_state == GPIO.LOW and current_clk == GPIO.HIGH:
                if GPIO.input(WREADY_PIN) == GPIO.HIGH:
                    i += 1 
                    if i >= len(data_bits): 
                        break
                GPIO.output(WDATA_PIN, data_bits[i])
            last_clk_state = current_clk
    else:
        while True:
            # Read the current clock state
            current_clk = GPIO.input(CLK_PIN)
            
            # Check for rising edge (LOW -> HIGH transition)
            if last_clk_state == GPIO.LOW and current_clk == GPIO.HIGH:
                # Check if the Arduino is ready to receive the next bit
                if GPIO.input(WREADY_PIN) == GPIO.HIGH:
                    i += 1  # Move to the next bit
                    if i >= len(data_bits):  # Stop if all bits are sent
                        break
                    # Send the next bit
                    GPIO.output(RDATA_PIN, data_bits[i])
            
            # Update the last clock state
            last_clk_state = current_clk
    GPIO.output(RREADY_PIN, GPIO.LOW)
    GPIO.output(WVALID_PIN, GPIO.LOW)

def process_received_bits(bits):
    global turn
    if not bits:
        return None

    is_move = bits[0]
    color = bits[1]
    payload = bits[2:]
    
    if is_move: #actual move
        if all(bit == 0 for bit in payload): #arduino telling pi to make the first move     
            stockfish[turn].set_position()
            best_move = stockfish[turn].get_best_move()
            turn = 1
            return move_to_bits(best_move)
        
        move_bits = int(''.join(map(str, payload[:14])), 2)
        move_str = bits_to_move(move_bits)
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
    first_write = 1
    # Initialize GPIO
    GPIO.setmode(GPIO.BCM)
    
    # Input Pins
    GPIO.setup(CLK_PIN, GPIO.IN)
    GPIO.setup(RREADY_PIN, GPIO.IN)
    GPIO.setup(WDATA_PIN, GPIO.IN)
    GPIO.setup(OVERWRITE_PIN, GPIO.IN)
    
    # Output Pins (Initialized to LOW)
    GPIO.setup(RVALID_PIN, GPIO.OUT)
    GPIO.setup(RDATA_PIN, GPIO.OUT)
    GPIO.setup(WREADY_PIN, GPIO.OUT)
    GPIO.output(RVALID_PIN, GPIO.LOW)
    GPIO.output(RDATA_PIN, GPIO.LOW)
    GPIO.output(WREADY_PIN, GPIO.LOW)
    
    # Initialize Stockfish
    stockfish = [Stockfish(
        path="/Users/gawtham3/Downloads/Stockfish-master/src/stockfish",
        depth=18,
        parameters={"Threads": 2, "Minimum Thinking Time": 30}
    ), Stockfish(
        path="/Users/gawtham3/Downloads/Stockfish-master/src/stockfish",
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
        response_bits = process_received_bits(received_bits)
        
        if response_bits is not None:
            write_to_arduino([int(b) for b in format(response_bits, '014b')])

if __name__ == "__main__":
    try:
        main()
    finally:
        GPIO.cleanup()
