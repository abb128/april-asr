import april_asr as april
import sys

def example_handler(is_final, tokens):
    s = ""
    for token in tokens:
        s = s + token.token
    
    if is_final:
        print("@"+s)
    else:
        print("-"+s)

def main():
    # Parse arguments
    args = sys.argv
    if len(args) != 3:
        print("Usage: " + args[0] + " /path/to/model.april /path/to/file.wav")
        return
    
    model_path = args[1]
    wav_file_path = args[2]

    # Load the model
    model = april.Model(model_path)

    # Print the metadata
    print("Name: " + model.get_name())
    print("Description: " + model.get_description())
    print("Language: " + model.get_language())

    # Create a session
    session = april.Session(model, example_handler)

    # Feed the audio data
    with open(wav_file_path, "rb") as f:
        session.feed_pcm16(f.read())

    # Flush to finish off
    session.flush()


if __name__ == "__main__":
    main()