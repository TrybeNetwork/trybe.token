//
// Created by Adam Clark on 9/30/18.
//

//#include <eosiolib/singleton.hpp>
#include "trybepresale.hpp"
#include <eosiolib/asset.hpp>
#include <eosiolib/eosio.hpp>

using namespace eosio;
using std::string;

class trybefunds : contract {


private:

    /***************************************************************************
    *                               T A B L E S
    ****************************************************************************/

    // @abi table trybepresale i64
    struct trybepresale {
        account_name owner;
        asset eos_amount;
        asset trybe_amount;
        time purchase_date;

        uint64_t primary_key() const { return owner; }

        EOSLIB_SERIALIZE( trybepresale, (owner)(eos_amount)(trybe_amount)(purchase_date) )

    };

    // @abi table presalestats i64
    struct presalestat {
        asset          totalavailable;
        asset          totalsold;
        account_name   issuer;

        uint64_t  primary_key() const { return totalavailable.symbol.name(); }

        EOSLIB_SERIALIZE( presalestat, (totalavailable)(totalsold)(issuer) )
    };

    typedef multi_index< N(trybepresale), trybepresale> presale_table;
    typedef multi_index< N(presalestats), presalestat>  presalestats_table;


    /***************************************************************************
    *                               F U N C T I O N S
    ****************************************************************************/



public:
    using contract::contract;
    trybefunds( name self ) : contract(self){}


    /**********************************************/
    /***                                        ***/
    /***             Token Transfers            ***/
    /***                                        ***/
    /**********************************************/

    void buy( const currency::transfer& t ){

        eosio_assert(t.quantity.symbol == string_to_symbol(4, "EOS"), "Token must be EOS");
        eosio_assert(t.quantity.is_valid(), "Token asset is not valid");
        eosio_assert(t.quantity.amount >= 1'0000, "Not enough tokens, need at 1 EOS");

        presale_table presale(_self,_self);

        auto presale_itr = presale.find(t.from);

        struct coinprice {
            asset  coin;
            asset  price;

            uint64_t primary_key()const { return coin.symbol.name(); }
        };

        typedef eosio::multi_index<N(coinprices), coinprice> coins;

        auto token_name  = asset(trybesale::EOS_MAX_SUPPLY, trybesale::EOS_SYMBOL);

        coins coin_table( N(trybenetwork), N(trybenetwork) );

        auto coin_itr = coin_table.get( token_name.symbol.name() );


        uint64_t eosprice = coin_itr.price.amount;

        uint64_t totalEOS = t.quantity.amount;

        auto newtrybe = asset( ( ( (totalEOS * eosprice ) / trybesale::TRYBE_SALE_PRICE  ) ) / 10000 , trybesale::TRYBESYMBOL);


        if( presale_itr == presale.end()  ){
            presale.emplace( _self, [&]( auto& s ) {
                s.owner        = t.from;
                s.eos_amount   = t.quantity;
                s.trybe_amount = newtrybe;
                s.purchase_date = now();
            });
        }else {

            presale.modify(presale_itr, 0, [&](auto &s) {
                s.eos_amount += t.quantity;
                s.trybe_amount += newtrybe;
                s.purchase_date = now();
            });
        }

        asset maximum_supply = asset(trybesale::TRYBE_MAX_SUPPLY, trybesale::TRYBESYMBOL);

        presalestats_table statstable(_self, maximum_supply.symbol.name());

        auto existing_itr = statstable.find( maximum_supply.symbol.name() );

        eosio_assert( existing_itr != statstable.end(), "Presale statistics entry not found" );

        if ( (existing_itr->totalsold + newtrybe ) > existing_itr->totalavailable  ){
            eosio_assert(false, "Not enough TRYBE available, please select a lower amount of EOS");
        }

        statstable.modify(existing_itr, 0, [&](auto &s) {
            s.totalsold += newtrybe;
        });

    }

    /*
    abi action
    void resetacct(account_name account){

        eosio_assert(false,"Disabled");

        presale_table presale(_self,_self);

        auto presale_itr = presale.find(account);

        eosio_assert(presale_itr != presale.end(),"fail" );

        presale.erase(presale_itr);

        asset maximum_supply = asset(trybesale::TRYBE_MAX_SUPPLY, trybesale::TRYBESYMBOL);

        presalestats_table statstable(_self, maximum_supply.symbol.name());

        auto existing_itr = statstable.find( maximum_supply.symbol.name() );

        eosio_assert( existing_itr != statstable.end(), "Presale statistics entry not found" );

        statstable.modify(existing_itr, 0, [&](auto &s) {
            s.totalsold = asset(0, trybesale::TRYBESYMBOL);
        });

    }*/


    // @abi action
    void setuppresale(){

        require_auth( _self );

        account_name issuer = _self;
        asset maximum_supply = asset(trybesale::TRYBE_MAX_SUPPLY, trybesale::TRYBESYMBOL);

        presalestats_table statstable(_self, maximum_supply.symbol.name());

        auto existing = statstable.find( maximum_supply.symbol.name() );

        //eosio_assert( existing == statstable.end(), "Sale settings already in place" );

        if ( existing == statstable.end() ) {
            statstable.emplace(_self, [&](auto &s) {
                s.totalavailable = asset(trybesale::TRYBE_MAX_SUPPLY, trybesale::TRYBESYMBOL);
                s.totalsold = asset(0, trybesale::TRYBESYMBOL);
                s.issuer = issuer;
            });
        }else{
            statstable.modify(existing, 0, [&](auto &s) {
                s.totalavailable = asset(trybesale::TEST_MAX_PRESALE, trybesale::TRYBESYMBOL);
            });
        }

    }

    void receivedTokens( const currency::transfer& t, account_name code ) {
        if( code == _self ){
            print("Contract sent money to itself?");
            return;
        }

        if( t.to == _self ) {
            if (code == N(eosio.token)) {

                if (t.memo == "TRYBE PRESALE"){
                    buy(t);
                }
            }
            else eosio_assert(false, "This contract only accepts EOS tokens");
        }
    }

    void apply( account_name contract, account_name action ) {
        if( action == N(transfer) ) {
            receivedTokens( unpack_action_data<eosio::currency::transfer>(), contract );
            return;
        }

        if( action == N(buy) ){
            eosio_assert(false, "Can't call buy directly");
        }

        if( contract != _self ) return;
        auto& thiscontract = *this;
        switch( action ) {
            EOSIO_API( trybefunds, (setuppresale) )
        };
    }
};


extern "C" {
[[noreturn]] void apply( uint64_t receiver, uint64_t code, uint64_t action ) {
    trybefunds c( receiver );
    c.apply( code, action );
    eosio_exit(0);
}
}
