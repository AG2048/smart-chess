import RPi.GPIO as GPIO
import time
from stockfish import Stockfish

# Pin Definitions (Global Constants)
CLK_PIN = 17
RVALID_PIN = 27
RREADY_PIN = 22
RDATA_PIN = 10
WVALID_PIN = 9
WREADY_PIN = 11
WDATA_PIN = 5
OVERWRITE_PIN = 6

# Global Game State
stockfish = None
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

def write_initial_sequence():
    pattern = [1, 0] * 7
    while True:
        if GPIO.input(OVERWRITE_PIN) == GPIO.HIGH:
            break
        for bit in pattern:
            GPIO.output(RDATA_PIN, bit)
            time.sleep(0.01)

def read_from_arduino():
    received_bits = []
    try:
        channel = GPIO.wait_for_edge(RVALID_PIN, GPIO.RISING, timeout=5000)
        if channel is None:
            print("Error: Timeout waiting for valid signal")
            return None

        for _ in range(16):
            if GPIO.wait_for_edge(CLK_PIN, GPIO.RISING, timeout=1000) is None:
                print("Error: Clock synchronization lost")
                return None
            
            time.sleep(0.0001)
            received_bits.append(GPIO.input(WDATA_PIN))
            GPIO.wait_for_edge(CLK_PIN, GPIO.FALLING, timeout=1000)

        return received_bits
        
    except KeyboardInterrupt:
        GPIO.cleanup()
        raise

def write_to_arduino(data_bits):
    GPIO.output(WREADY_PIN, GPIO.HIGH)
    for bit in data_bits:
        GPIO.output(RDATA_PIN, bit)
        while GPIO.input(CLK_PIN) == GPIO.LOW:
            time.sleep(0.01)
        while GPIO.input(CLK_PIN) == GPIO.HIGH:
            time.sleep(0.01)
    GPIO.output(WREADY_PIN, GPIO.LOW)

def process_received_bits(bits):
    global turn
    if not bits:
        return None

    is_move = bits[0]
    color = bits[1]
    payload = bits[2:]
    
    if is_move:
        if all(bit == 0 for bit in payload):
            stockfish.set_position()
            best_move = stockfish.get_best_move()
            turn = 1
            return move_to_bits(best_move)
        
        move_bits = int(''.join(map(str, payload[:14])), 2)
        move_str = bits_to_move(move_bits)
        stockfish.make_moves_from_current_position([move_str])
        
        if is_computer[not turn]:
            best_move = stockfish.get_best_move()
            return move_to_bits(best_move)
        turn = 1 - turn
    else:
        stockfish.set_position()
        is_human = all(bit == 1 for bit in payload[:14])
        difficulty = int(''.join(map(str, payload[:14])), 2)
        is_computer[color] = not is_human
        if not is_human:
            stockfish.update_engine_parameters({"Skill Level": difficulty})
        turn = 0
    return None

def main():
    global stockfish, turn
    
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
    stockfish = Stockfish(
        path="/Users/gawtham3/Downloads/Stockfish-master/src/stockfish",
        depth=18,
        parameters={"Threads": 2, "Minimum Thinking Time": 30}
    )
    
    # Initial sequence
    write_initial_sequence()
    stockfish.set_position()
    
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
