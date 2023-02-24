import april_asr as april
import time
import sys

# Creates a handler function that prints with a numbered prefix
def indexed_handler(i):
    def handler(is_final, tokens):
        s = ""
        for token in tokens:
            s = s + token.token
        
        if is_final:
            print(f'{i}: @{s}')
        else:
            print(f'{i}: -{s}')
    
    return handler

def run(model_path, wav_file_path):
    # Load the model
    model = april.Model(model_path)

    # Print the metadata
    print("Name: " + model.get_name())
    print("Description: " + model.get_description())
    print("Language: " + model.get_language())

    # Create 5 sessions
    sessions = []
    for i in range(5):
        session = april.Session(model, indexed_handler(i), asynchronous=True)
        sessions.append(session)
    
    # Feed the audio data
    CHUNK_SIZE = 2400
    with open(wav_file_path, "rb") as f:
        while True:
            data = f.read(CHUNK_SIZE)
            
            if len(data) == 0: break

            for session in sessions:
                session.feed_pcm16(data)
            
            time.sleep(CHUNK_SIZE / model.get_sample_rate())
    
    # Flush to finish off
    for session in sessions:
        session.flush()

def main():
    # Parse arguments
    args = sys.argv
    if len(args) != 3:
        print("Usage: " + args[0] + " /path/to/model.april /path/to/file.wav")
    else:    
        run(args[1], args[2])

if __name__ == "__main__":
    main()