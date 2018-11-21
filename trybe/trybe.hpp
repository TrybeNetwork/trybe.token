#pragma once

#include <eosiolib/asset.hpp>
#include <eosiolib/eosio.hpp>
#include <eosiolib/vector.hpp>

namespace eosiosystem {
  class system_contract;
}

namespace eosio {

  using std::string;
  using std::vector;
  static uint64_t     SYMBOL = string_to_symbol(4, "TRYBE");
  static int64_t      MAX_SUPPLY = 10'000'000'000'0000;

  class trybe : public eosio::contract {

  public:

    trybe(account_name self) : eosio::contract(self) {}

    /// @abi action
    void add(account_name subscriber, uint8_t status);

//    /// @abi action
//    void seteostoken(asset coin_name, double eos_price, double usd_price);

    /// @abi action
    void seteostoken( const std::vector<asset> &tokenname_vtr,
                      const std::vector<double> &eosprice_vtr,
                      const std::vector<double> &usdprice_vtr);

    /// @abi action
    void remove(account_name subscriber);

    /// @abi action
    void confirm(account_name subscriber, uint8_t status);

//    /// @abi action
//    void gettokens();

    /// @abi action
    void create();

    /// @abi action
    void claim( account_name claimer );

    /// @abi action
    void issue( account_name to, asset quantity, string memo );

    /// @abi action
    void transfer( account_name from,
                   account_name to,
                   asset        quantity,
                   string       memo );


    inline asset get_supply( symbol_name sym )const;

    inline asset get_balance( account_name owner, symbol_name sym )const;

  private:

    /// @abi table subscribers i64
    struct subscriber {
      account_name account;
      uint8_t status;
      bool accepted;
      time starttime;

      uint64_t primary_key() const { return account; }

      EOSLIB_SERIALIZE(subscriber, (account) (status) (starttime) (accepted))
    };

    typedef multi_index<N(subscribers), subscriber> subscribers;

    /// @abi table accounts i64
    struct account {
        asset    balance;

        uint64_t primary_key()const { return balance.symbol.name(); }
        EOSLIB_SERIALIZE(account, (balance))
    };

    /// @abi table stat i64
    struct currencystat {
        asset          supply;
        asset          max_supply;
        account_name   issuer;

        uint64_t primary_key()const { return supply.symbol.name(); }

        EOSLIB_SERIALIZE(currencystat, (supply) (max_supply) (issuer))
    };

    typedef eosio::multi_index<N(accounts), account> accounts;
    typedef eosio::multi_index<N(stat), currencystat> stats;

    /// @abi table eostoken i64
    struct tokenprices {
        asset          coin_name;
        double         eos_price;
        double         usd_price;

        uint64_t primary_key()const { return coin_name.symbol.name(); }

        EOSLIB_SERIALIZE(tokenprices, (coin_name) (eos_price) (usd_price))
    };

    typedef eosio::multi_index<N(eostoken), tokenprices> eostoken;

    void sub_balance( account_name owner, asset value );
    void add_balance( account_name owner, asset value, account_name ram_payer );

  };

  asset trybe::get_supply( symbol_name sym )const
  {
      stats statstable( _self, sym );
      const auto& st = statstable.get( sym );
      return st.supply;
  }

  asset trybe::get_balance( account_name owner, symbol_name sym )const
  {
      accounts accountstable( _self, owner );
      const auto& ac = accountstable.get( sym );
      return ac.balance;
  }

} /// namespace eosio

