import RPi.GPIO as GPIO
import time
from stockfish import Stockfish

# Stockfish setup
stockfish = Stockfish(path="/Users/gawtham3/Downloads/Stockfish-master/src/stockfish", depth=18, parameters={"Threads": 2, "Minimum Thinking Time": 30})

# Pin definitions
CLK_PIN = 17    # Clock (input from Arduino)
RVALID_PIN = 27  # Valid (output to Arduino)
RREADY_PIN = 22  # Ready (input from Arduino)
RDATA_PIN = 10   # Data (output to Arduino)
WVALID_PIN = 9   # Valid (input from Arduino)
WREADY_PIN = 11  # Ready (output to Arduino)
WDATA_PIN = 5    # Data (input from Arduino)
OVERWRITE_PIN = 6  # Overwrite (input from Arduino)

"""
Initially, set all pin types to LOW (all the output pins)
Then, start the "write" protocol, to write full repeating 1 and 0s.
If the "overwrite" pin is set to HIGH, then break out of the write loop.
Begin the "read" protocol.

Inside READ:
We read 16 bits.
MSB: 1 for a move, 0 for starting an agent. 
next bit: 0 for white, 1 for black
(Arduino always write the data for BOTH sides)
next 14 bits: difficulty of agent (0 indexed) (If arduino write all 1s, then it is a human side)
OR
next 14 bits: move (12 bits for from and to index, and 2 bits for promotion)

If we are setting up agent: 
- Reset the stockfish object
- Remember if the side is a computer or human


Main loop:
- Do the write 1010101010... loop
- READ until it's a move (when it's not a move, turn is always set to 0)
- If it is a move, then:
    - If it's all 0s (It's arduino telling the pi to make the first move)
        - Do WRITE: write agent[0]'s best move
        - turn = 1
    - Else:
        - Update the stockfish object for both players. 
        - Write: agent[!turn]'s best move -- ONLY IF !turn is a computer.
        - (note if !turn isn't a computer, then skip write)
    turn = !turn


For arduino:
On game start: sends a AI agent initialize signal for both colours. 
IF colour 0 is an agent, sends 000000000000000...
READ FROM PI (when gathering moves)
at turn ending: write the move to the pi (do not write if that move was a checkmate)
repeat
"""


# GPIO setup
GPIO.setmode(GPIO.BCM)
GPIO.setup(CLK_PIN, GPIO.IN)
GPIO.setup(VALID_PIN, GPIO.OUT)
GPIO.setup(READY_PIN, GPIO.IN)
GPIO.setup(DATA_PIN, GPIO.OUT)
GPIO.output(VALID_PIN, GPIO.LOW)

# --- Encoding/Decoding Functions ---
def move_to_bits(move_str):
    if len(move_str) != 4:
        raise ValueError("Move string must be 4 characters (e.g., 'e1e3')")
    
    start_file = ord(move_str[0].lower()) - ord('a')
    start_rank = int(move_str[1]) - 1
    end_file = ord(move_str[2].lower()) - ord('a')
    end_rank = int(move_str[3]) - 1

    # Validate ranges (0-7)
    for val in [start_file, start_rank, end_file, end_rank]:
        if not 0 <= val <= 7:
            raise ValueError("Invalid move value")

    # Pack into 14 bits
    bits = (start_file << 11) | (start_rank << 8) | (end_file << 5) | (end_rank << 2)
    return bits

def bits_to_move(bits):
    start_file = (bits >> 11) & 0b111
    start_rank = (bits >> 8) & 0b111
    end_file = (bits >> 5) & 0b111
    end_rank = (bits >> 2) & 0b111

    move_str = (
        chr(start_file + ord('a')) + str(start_rank + 1) + 
        chr(end_file + ord('a')) + str(end_rank + 1)
    return move_str

def read_from_arduino():
    last_clk = GPIO.input(CLK_PIN)
    receiving = False
    databank = []
    i = 0

    GPIO.output(READY_PIN, GPIO.HIGH)
    print("READY SET")
    valid = 0

    while True:
        current_data = GPIO.input(DATA_PIN)
        current_clk = GPIO.input(CLK_PIN)
        temp_valid = GPIO.input(VALID_PIN)

        if last_clk == GPIO.LOW and current_clk == GPIO.HIGH:
            if temp_valid == GPIO.HIGH:
                receiving = True
                GPIO.output(READY_PIN, GPIO.LOW)
                print("READY CLEARED")

            if receiving:
                databank.append(current_data)
                i += 1

        if i >= 14:
            # Convert bits to integer
            number = sum(bit << idx for idx, bit in enumerate(databank))
            move_str = bits_to_move(number)
            print(f"Received move: {move_str}")

            # Update Stockfish
            stockfish.make_moves_from_current_position([move_str])
            print(stockfish.get_board_visual())

            # Get best move and send back
            best_move = stockfish.get_best_move()
            print(f"Stockfish response: {best_move}")
            best_bits = move_to_bits(best_move)
            write_to_arduino(best_bits)
            
            return databank

        last_clk = current_clk

def write_to_arduino(number):
    data = [(number >> i) & 1 for i in range(14)]
    i = 0
    last_clk_state = GPIO.input(CLK_PIN)
    GPIO.output(DATA_PIN, data[i])

    while True:
        current_clk = GPIO.input(CLK_PIN)
        ready_state = GPIO.input(READY_PIN)

        if last_clk_state == GPIO.LOW and current_clk == GPIO.HIGH:
            if ready_state == GPIO.HIGH:
                i += 1
                if i >= 14:
                    break
                GPIO.output(DATA_PIN, data[i])

        last_clk_state = current_clk

def main():
    stockfish.set_position()  # Reset board
    while True:
        read_from_arduino()  # Process moves continuously

if __name__ == "__main__":
    main()
