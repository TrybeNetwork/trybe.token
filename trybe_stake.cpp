/* Title:  trybe_founders_stake.cpp
*  Description: staking contract for Trybe Founders Group
*  Author: Adam Clark
*  Telegram: @aclark80
*/

#include <eosiolib/transaction.hpp>

namespace trybe {

    using std::to_string;

    static constexpr time USER_REFUND_DELAY     = 60;   // 60 seconds
    static constexpr time FOUNDER_REFUND_DELAY  = 60;   // 60 seconds

    /***************************************************************************
    *                               T A B L E S
    ****************************************************************************/


    // @abi table acctstaked i64
    struct stakedaccts {
        account_name  account;
        asset         amount;

        uint64_t  primary_key() const { return account; }

        // explicit serialization macro is not necessary,
        // used here only to improve compilation time
        EOSLIB_SERIALIZE( stakedaccts, (account)(amount) )
    };

    // @abi table refunds i64
    struct refund {
        account_name  owner;
        time          request_time;
        asset         amount;

        uint64_t  primary_key() const { return owner; }

        // explicit serialization macro is not necessary,
        // used here only to improve compilation time
        EOSLIB_SERIALIZE( refund, (owner)(request_time)(amount) )
    };

    // @abi table founders i64
    struct founder_table {
        account_name      account;

        uint64_t  primary_key() const { return account; }

        EOSLIB_SERIALIZE( founder_table, (account) )
    };

    // @abi table trybestaked i64
    struct trybestaked {
        account_name  owner;
        account_name  from;
        asset         total_staked_trybe;
        time          initial_staked_time;

        uint64_t  primary_key() const { return owner; }
        uint64_t find_from() const { return from; }

        EOSLIB_SERIALIZE( trybestaked, (owner)(from)(total_staked_trybe)(initial_staked_time) )


    };

    typedef multi_index< N(refunds), refund>            refunds_table;
    typedef multi_index< N(acctstaked), stakedaccts>    stakedaccts_table;
    typedef multi_index< N(founders), founder_table>    founders_table;
    typedef multi_index< N(trybestaked), trybestaked,
                indexed_by< N(from), const_mem_fun<trybestaked,uint64_t,
                &trybestaked::find_from>>
                         > trybe_staked_table;

    /****************************************************************************
    *                             A C T I O N S
    *****************************************************************************/

    // @abi action
    void token::fndrupdate( account_name founder){

        require_auth2(_self,N(founders));

        eosio_assert( is_account( founder ), "to account does not exist");

        founders_table founder_table( _self, _self );

        auto founder_itr = founder_table.find( founder );

        bool founderactive = check_founder(founder);

        eosio_assert(  founderactive == false , "Founder account already added"  );

        founder_table.emplace( _self, [&]( auto& new_entry ){
            new_entry.account = founder;
        });

    }

    // @abi action
    void token::refundtrybe( account_name account, bool refund_or_cancel) {

        require_auth(account);

        refunds_table refundtrybe( _self, account);

        auto refund_itr = refundtrybe.find( account );

        eosio_assert(refund_itr!=refundtrybe.end(),"No refunds found for account");

        time refund_delay = ( check_founder(account) ) ? FOUNDER_REFUND_DELAY : USER_REFUND_DELAY;


        if ( now() < refund_itr->request_time + refund_delay ) {
            string err = "refund is not available yet " + to_string( (refund_itr->request_time + refund_delay) - now() )
                         + " seconds remaining";
            eosio_assert( false, err.c_str() );
        }

        auto refund_amount = refund_itr->amount;

        refundtrybe.erase(refund_itr);

        add_balance( account, refund_amount , account);


    }

    // @abi action
    void token::stake( account_name from, account_name to, asset total_trybe_to_stake, bool transfer = false ) {

        // Validate passed values

        //Check sender has signed the transaction
        require_auth( from );

        asset current_staked_trybe;

        eosio_assert( is_account( to ), "receiving account does not exist");
        eosio_assert( total_trybe_to_stake.is_valid(), "invalid asset details offered");
        eosio_assert( total_trybe_to_stake >= asset(0, trybe::SYMBOL), "must stake a positive amount" );

        trybe_staked_table stakedtrybe( _self, from );
        accounts        from_accounts( _self, from );

        auto trybe_account_itr = from_accounts.find( total_trybe_to_stake.symbol.name() );


        auto staked_trybe_itr = stakedtrybe.find( from );

        if ( staked_trybe_itr == stakedtrybe.end() )
            current_staked_trybe = asset(0, SYMBOL);
        else
            current_staked_trybe = staked_trybe_itr->total_staked_trybe;

        eosio_assert( total_trybe_to_stake <= (trybe_account_itr->balance ),
                      "not enough liquid TRYBE in account" );

        auto owner = (transfer == 1) ? to : from;

        stake_trybe( from, to, total_trybe_to_stake , owner);

    }

    // @abi action
    void token::unstake( account_name owner, account_name receiver, asset total_trybe_to_unstake ) {

        bool is_founder = check_founder( owner );

        //eosio_assert( ! is_founder,"Founders cannot unstake until Jan 2019");

        //Check sender has signed the transaction
        require_auth( owner );

        asset current_staked_trybe;

        eosio_assert( is_account( receiver ), "receiving account does not exist");
        eosio_assert( total_trybe_to_unstake.is_valid(), "invalid asset details offered");
        eosio_assert( total_trybe_to_unstake >= asset(0, trybe::SYMBOL), "must stake a positive amount" );

        trybe_staked_table stakedtrybe( _self, owner );
        accounts        from_accounts( _self, owner );

        auto staked_trybe_itr = stakedtrybe.find( owner );

        eosio_assert(staked_trybe_itr != stakedtrybe.end(),"No staking entry found ");


        auto totalstaked = staked_trybe_itr->total_staked_trybe;

        eosio_assert(totalstaked>total_trybe_to_unstake,"You do not have enough staked TRYBE");

        stakedtrybe.modify( staked_trybe_itr, 0, [&]( auto& tot ) {
            tot.total_staked_trybe    -= total_trybe_to_unstake;
        });

        refunds_table refundtrybe( _self, owner );

        auto refund_itr = refundtrybe.find( owner );

        eosio_assert(refund_itr == refundtrybe.end(),"Pending refund in progress");

        refundtrybe.emplace( owner, [&]( auto& refund ) {
            refund.owner         = owner;
            refund.request_time  = now();
            refund.amount        = total_trybe_to_unstake;
        });

        update_staked_accounts(owner,-total_trybe_to_unstake);

        transaction out{};

        out.actions.emplace_back(permission_level{owner, N(active)}, N(trybenetwork), N(refundtrybe), std::make_tuple(owner,1));
        out.delay_sec = (  ( check_founder(owner) ) ? FOUNDER_REFUND_DELAY : USER_REFUND_DELAY );
        out.send(owner, owner);

    }


    /****************************************************************************
    *                             F U N C T I O N S
    *****************************************************************************/

    bool token::check_founder( account_name founder ){

        founders_table founder_table( _self, _self );

        auto founder_itr = founder_table.find( founder );

        return ( founder_itr == founder_table.end() ) ? false : true;
    }

    void token::update_staked_accounts(account_name account, asset amount ){

        stakedaccts_table stakedaccts( _self, _self);

        auto stakedaccts_itr = stakedaccts.find( account );

        if ( stakedaccts_itr == stakedaccts.end() ){

            stakedaccts.emplace( account, [&]( auto& stktot ) {
                stktot.account                 = account;
                stktot.amount                  = amount;
            });

        }else{
            stakedaccts.modify( stakedaccts_itr, 0, [&]( auto& stktot ) {
                stktot.amount    += amount;
            });

            if (stakedaccts_itr->amount == asset(0, SYMBOL)){
                stakedaccts.erase(stakedaccts_itr);
            }
        }
    }

    asset inline token::get_staked_trybe( account_name account ){


        trybe_staked_table stakedtrybe( _self, account );

        auto trybe_staked_itr = stakedtrybe.find( account );


        if ( trybe_staked_itr == stakedtrybe.end() )
            return asset(0, SYMBOL);
        else
            return trybe_staked_itr->total_staked_trybe;

    }


    void inline token::stake_trybe( account_name& from,
                                              account_name& receiver,
                                              const asset&  stake_trybe_delta,
                                              account_name owner) {


        // create an instance to table struct with the scope of the sending account
        trybe_staked_table stakedtrybe( _self, receiver );

        // Add an iter pointing to the entry in the table
        auto staked_trybe_itr = stakedtrybe.find( owner );


        if( staked_trybe_itr ==  stakedtrybe.end() ) {
            staked_trybe_itr = stakedtrybe.emplace( from, [&]( auto& tot ) {
                tot.owner                 = owner;
                tot.from                  = from;
                tot.total_staked_trybe    = stake_trybe_delta;
                tot.initial_staked_time   = now();
            });
        } else {
            stakedtrybe.modify( staked_trybe_itr, 0, [&]( auto& tot ) {
                tot.total_staked_trybe    += stake_trybe_delta;
            });
        }
        eosio_assert( asset(0, SYMBOL) <= staked_trybe_itr->total_staked_trybe, "insufficient staked TRYBE" );

        if ( staked_trybe_itr->total_staked_trybe == asset(0, SYMBOL) ) {
            stakedtrybe.erase( staked_trybe_itr );
        }

        update_staked_accounts(owner,stake_trybe_delta);

        sub_balance(from,stake_trybe_delta );

    }
}  /// namespace trybe
