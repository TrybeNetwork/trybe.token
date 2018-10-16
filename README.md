# trybe.token
Preliminary token contract based on RIDL

* Adds airgrab claiming using the command:

`cleos push action trybenetwork claim '{"claimer":"myaccount"}' -p myaccount`

# trybepresale
Presale contract based on the RIDL token sale

The buy action cannot be called directy, it can only be accessed by transfering EOS tokens to the trybepresale account with a memo of "TRYBE PRESALE" or by visting https://wallet.trybe.one.
