package aprilasr;

public class Token {
    private String value;
    private float logprob;
    private int flags;
    private double time;

    public String getValue(){
        return value;
    }

    public float getLogProb(){
        return logprob;
    }

    public boolean isWordBoundary(){
        return (flags & 0b1) != 0;
    }

    public double getTime() {
        return time;
    }

    public static String concat(Token[] tokens){
        String s = "";
        for(Token token : tokens){
            s += token.getValue();
        }

        return s.stripLeading();
    }

    Token(AprilAsrNative.AprilToken token){
        this.flags = token.flags;
        this.time = token.time_ms.doubleValue() / 1000.0;
        this.value = new String(token.token);
        this.logprob = token.logprob;
    }
}
