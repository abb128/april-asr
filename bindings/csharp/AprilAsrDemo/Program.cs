using AprilAsr;


internal class Program {
    private static void Run(string modelPath, string wavFilePath) {
        // Load the model and print metadata
        var model = new AprilModel(modelPath);
        Console.WriteLine("Name: " + model.Name);
        Console.WriteLine("Description: " + model.Description);
        Console.WriteLine("Language: " + model.Language);

        // Create the session with an inline callback
        var session = new AprilSession(model, (kind, tokens) => {
            if (tokens == null) return;

            string s = "";
            foreach(AprilToken token in tokens) {
                s += token.Token;
            }

            Console.WriteLine(s);
        });

        // Read the file data (assumes wav file is 16-bit PCM wav)
        var fileData = File.ReadAllBytes(wavFilePath);
        short[] shorts = new short[fileData.Length / 2];
        Buffer.BlockCopy(fileData, 0, shorts, 0, fileData.Length);

        // Feed the data and flush
        session.FeedPCM16(shorts, shorts.Length);
        session.Flush();
    }

    private static void Main(string[] args)
    {
        var modelPath = "";
        var wavFilePath = "";

        if (args.Length >= 2) {
            modelPath = args[0];
            wavFilePath = args[1];
        } else {
            Console.Write("Enter Model Path: ");
            modelPath = Console.ReadLine() ?? "";

            Console.Write("Enter Wave File Path: ");
            wavFilePath = Console.ReadLine() ?? "";
        }

        Run(modelPath, wavFilePath);
    }
}