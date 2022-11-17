# .april file format

The file contains a version, a header containing name, description and
model kind, and offsets. The rest of the file contains the networks and
the parameters.

Some structures:
```c
struct File {
    char magic[8]; // = "APRILMDL"
    uint32_t version; // = 1
    uint64_t header_size;
    Header header;
    void contents[]; // the rest of the file
};

struct Header {
    // https://en.wikipedia.org/wiki/IETF_language_tag
    char language_tag[8];

    uint64_t name_length;
    char name[]; // of size `name_length`

    uint64_t description_length;
    char description[]; // of size `description_length`

    ModelType model;
    ArchiveFileEntry params;

    uint64_t network_count; // = 3 for APRILMDL_LSTM_TRANSDUCER_STATELESS
    ArchiveFileEntry networks[]; // of size `network_count`
};

enum ModelType : uint32_t {
    APRILMDL_UNKNOWN = 0,

    /* networks:
        [0]: encoder
        [1]: decoder
        [2]: joiner   */
    APRILMDL_LSTM_TRANSDUCER_STATELESS = 1
};

struct ArchiveFileEntry {
    uint64_t offset; // relative to start of file
    uint64_t size;
};
```


## params

```c
struct Params {
    char magic[8]; // "PARAMS\0\0"
    int32_t batch_size;
    int32_t segment_size;
    int32_t mel_features;
    int32_t samplerate;
    int32_t frame_shift_ms;
    int32_t frame_length_ms;
    int32_t round_pow2;
    int32_t mel_low;
    int32_t mel_high; // may be 0, which implies SAMPLERATE/2
    int32_t snip_edges;
    
    int32_t token_count; // example: 500
    int32_t blank_token_id; // between 0 and token_count, usually 0

    Token tokens[]; // of size `token_count`
};


struct Token {
    int32_t token_length;
    char token[]; // of size `token_length`
};
```