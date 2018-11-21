#include "trybe.hpp"
#include <iostream>
#include <eosiolib/eosio.hpp>
#include <eosiolib/print.hpp>

using namespace std;

namespace eosio {

  void trybe::create(){
      require_auth( _self );

      account_name issuer = _self;
      asset maximum_supply = asset(MAX_SUPPLY, SYMBOL);

      stats statstable( _self, maximum_supply.symbol.name() );
      auto existing = statstable.find( maximum_supply.symbol.name() );
      eosio_assert( existing == statstable.end(), "token with symbol already exists" );

      statstable.emplace( _self, [&]( auto& s ) {
          s.supply.symbol = maximum_supply.symbol;
          s.max_supply    = maximum_supply;
          s.issuer        = issuer;
      });
  }


  void trybe::claim( account_name claimer ) {
      require_auth( claimer );

      accounts acnts( _self, claimer );

      asset empty = asset(0'0000, SYMBOL);

      auto existing = acnts.find( empty.symbol.name() );
      eosio_assert(existing == acnts.end(), "User already has a balance");

      acnts.emplace( claimer, [&]( auto& a ){
          a.balance = empty;
      });
  }


  void trybe::issue( account_name to, asset quantity, string memo ) {
      auto sym = quantity.symbol;
      eosio_assert( sym.is_valid(), "invalid symbol name" );
      eosio_assert( memo.size() <= 256, "memo has more than 256 bytes" );

      auto sym_name = sym.name();
      stats statstable( _self, sym_name );
      auto existing = statstable.find( sym_name );
      eosio_assert( existing != statstable.end(), "token with symbol does not exist, create token before issue" );
      const auto& st = *existing;

      require_auth( st.issuer );
      eosio_assert( quantity.is_valid(), "invalid quantity" );
      eosio_assert( quantity.amount > 0, "must issue positive quantity" );

      eosio_assert( quantity.symbol == st.supply.symbol, "symbol precision mismatch" );
      eosio_assert( quantity.amount <= st.max_supply.amount - st.supply.amount, "quantity exceeds available supply");

      statstable.modify( st, 0, [&]( auto& s ) {
          s.supply += quantity;
      });

      add_balance( st.issuer, quantity, st.issuer );

      if( to != st.issuer ) {
          SEND_INLINE_ACTION( *this, transfer, {st.issuer,N(active)}, {st.issuer, to, quantity, memo} );
      }
  }

  void trybe::transfer( account_name from, account_name to, asset quantity, string memo ) {
      eosio_assert( from != to, "cannot transfer to self" );
      require_auth( from );
      eosio_assert( is_account( to ), "to account does not exist");
      auto sym = quantity.symbol.name();
      stats statstable( _self, sym );
      const auto& st = statstable.get( sym );

      require_recipient( from );
      require_recipient( to );

      eosio_assert( quantity.is_valid(), "invalid quantity" );
      eosio_assert( quantity.amount > 0, "must transfer positive quantity" );
      eosio_assert( quantity.symbol == st.supply.symbol, "symbol precision mismatch" );
      eosio_assert( memo.size() <= 256, "memo has more than 256 bytes" );


      sub_balance( from, quantity );
      add_balance( to, quantity, from );
  }

  void trybe::sub_balance( account_name owner, asset value ) {
      accounts from_acnts( _self, owner );

      const auto& from = from_acnts.get( value.symbol.name(), "no balance object found" );
      eosio_assert( from.balance.amount >= value.amount, "overdrawn balance" );


      if( from.balance.amount == value.amount ) {
          from_acnts.erase( from );
      } else {
          from_acnts.modify( from, owner, [&]( auto& a ) {
              a.balance -= value;
          });
      }
  }

  void trybe::add_balance( account_name owner, asset value, account_name ram_payer ) {
      accounts to_acnts( _self, owner );
      auto to = to_acnts.find( value.symbol.name() );
      if( to == to_acnts.end() ) {
          to_acnts.emplace( ram_payer, [&]( auto& a ){
              a.balance = value;
          });
      } else {
          to_acnts.modify( to, 0, [&]( auto& a ) {
              a.balance += value;
          });
      }
  }

//  void trybe::seteostoken(asset  coin_name,
//                          double eos_price,
//                          double usd_price) {
//      require_auth( _self );
//
//      account_name issuer = _self;
//      eostoken statstable( _self, _self );
//      auto existing = statstable.find( coin_name.symbol.name() );
//
//      if(existing != statstable.end()){
//          statstable.modify(existing, 0, [&](auto &w) {
//            w.eos_price     = eos_price;
//            w.usd_price     = usd_price;
//          });
//      } else {
//          statstable.emplace(_self, [&](auto &w) {
//            w.eos_price     = eos_price;
//            w.usd_price     = usd_price;
//            w.coin_name     = coin_name;
//          });
//      }
//  }

  void trybe::seteostoken(
                          const std::vector<asset> &tokenname_vtr,
                          const std::vector<double> &eosprice_vtr,
                          const std::vector<double> &usdprice_vtr
                          ) {
      require_auth( _self );

      eosio_assert( tokenname_vtr.size() == eosprice_vtr.size(), "tokenname and eosprice vectors have different size" );
      eosio_assert( tokenname_vtr.size() == usdprice_vtr.size(), "tokenname and usdprice vectors have different size" );

      account_name issuer = _self;
      eostoken statstable( _self, _self );
      for(int i = 0; i < tokenname_vtr.size(); i++){
          auto existing = statstable.find( tokenname_vtr[i].symbol.name() );

          if(existing != statstable.end()){
              statstable.modify(existing, 0, [&](auto &w) {
                w.eos_price     = eosprice_vtr[i];
                w.usd_price     = usdprice_vtr[i];
              });
          } else {
              statstable.emplace(_self, [&](auto &w) {
                w.eos_price     = eosprice_vtr[i];
                w.usd_price     = usdprice_vtr[i];
                w.coin_name     = tokenname_vtr[i];
              });
          }
      }
  }

  void trybe::add(account_name subscriber, uint8_t status) {
      require_auth(subscriber);

      subscribers subtable(_self, _self);
      auto existing = subtable.find(subscriber);
      eosio_assert(existing == subtable.end(), "Rows for this subscriber already exist");

      subtable.emplace(_self, [&](auto &t) {
        t.account = subscriber;
        t.status = status;
        t.accepted = false;
        t.starttime = now();
      });
  }

  void trybe::remove(account_name subscriber) {
      require_auth(_self);

      subscribers subtable(_self, _self);
      const auto &row = subtable.get(subscriber, "No subscribe found for this subscriber");
      subtable.erase(row);
  }

  void trybe::confirm(account_name subscriber, uint8_t status) {
      require_auth(_self);

      subscribers subtable(_self, _self);
      auto existing = subtable.find(subscriber);

      eosio_assert(existing != subtable.end(), "this subscriber does not exist");

      subtable.modify(existing, 0, [&](auto &s) {
        s.starttime = now();
        s.status = status;
        s.accepted = true;
      });
  }

//  void trybe::gettokens() {
//      require_auth(_self);
//      eostoken tokens(_self, _self);
//      for( const auto& item : tokens ) {
////          string coin_name = std::to_string(item.max_supply.symbol.name());
//          print(
//                "coin_name=", item.coin_name,
//                ",eos_price=", item.eos_price,
//                ",usd_price=", item.usd_price,
//                "\n"
//                );
//      }
//  }


} /// namespace eosio

EOSIO_ABI( eosio::trybe, (add)(remove)(seteostoken)(confirm)(create)(claim)(issue)(transfer))