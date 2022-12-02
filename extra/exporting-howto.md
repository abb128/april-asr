# Exporting April model

Currently only `lstm_transducer_stateless2` model is tested and supported.

To export, copy export-april.py to the `lstm_transducer_stateless2` directory and call it as instructed.

For example:
```sh
$ lstm_transducer_stateless2/export-april.py \
    --epoch 30 \
    --avg 15 \
    --exp-dir lstm_transducer_stateless2/exp \
    --name "Joe's Epic Model" \
    --description "This is my model that I worked very hard to create. For more information visit https://example.com/" \
    --language "en-us"
```

This will produce a .april file in the exp-dir. This can be loaded by libaprilasr.