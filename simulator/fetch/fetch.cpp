/*
 * fetch.cpp - simulator of fetch unit
 * Copyright 2015-2018 MIPT-MIPS
 */

#include <infra/config/config.h>
 
#include "fetch.h"

namespace config {
    static Value<std::string> bp_mode = { "bp-mode", "dynamic_two_bit", "branch prediction mode"};
    static Value<uint32> bp_size = { "bp-size", 128, "BTB size in entries"};
    static Value<uint32> bp_ways = { "bp-ways", 16, "number of ways in BTB"};
} // namespace config

template <typename ISA>
Fetch<ISA>::Fetch(bool log) : Log( log)
{
    wp_fetch_2_decode = make_write_port<Instr>("FETCH_2_DECODE", PORT_BW, PORT_FANOUT);
    rp_decode_2_fetch_stall = make_read_port<bool>("DECODE_2_FETCH_STALL", PORT_LATENCY);

    rp_fetch_flush = make_read_port<bool>("MEMORY_2_ALL_FLUSH", PORT_LATENCY);

    rp_memory_2_fetch_target = make_read_port<Addr>("MEMORY_2_FETCH_TARGET", PORT_LATENCY);

    wp_target = make_write_port<Addr>("TARGET", PORT_BW, PORT_FANOUT);
    rp_target = make_read_port<Addr>("TARGET", PORT_LATENCY);

    wp_hold_pc = make_write_port<Addr>("HOLD_PC", PORT_BW, PORT_FANOUT);
    rp_hold_pc = make_read_port<Addr>("HOLD_PC", PORT_LATENCY);

    rp_core_2_fetch_target = make_read_port<Addr>("CORE_2_FETCH_TARGET", PORT_LATENCY);

    rp_memory_2_bp = make_read_port<BPInterface>("MEMORY_2_FETCH", PORT_LATENCY);

    BPFactory bp_factory;
    bp = bp_factory.create( config::bp_mode, config::bp_size, config::bp_ways);
}

template <typename ISA>
Addr Fetch<ISA>::get_PC( Cycle cycle) 
{
    /* receive flush and stall signals */
    const bool is_flush = rp_fetch_flush->is_ready( cycle) && rp_fetch_flush->read( cycle);
    const bool is_stall = rp_decode_2_fetch_stall->is_ready( cycle) && rp_decode_2_fetch_stall->read( cycle);

    /* Receive all possible PC */
    const Addr external_PC = rp_core_2_fetch_target->is_ready( cycle) ? rp_core_2_fetch_target->read( cycle) : 0;
    const Addr hold_PC     = rp_hold_pc->is_ready( cycle) ? rp_hold_pc->read( cycle) : 0;
    const Addr flushed_PC  = rp_memory_2_fetch_target->is_ready( cycle) ? rp_memory_2_fetch_target->read( cycle) : 0;
    const Addr target_PC   = rp_target->is_ready( cycle) ? rp_target->read( cycle) : 0;

    /* Multiplexing */
    if ( external_PC != 0)
        return external_PC;

    if ( is_flush)
        return flushed_PC;

    if ( !is_stall)
        return target_PC;

    if ( hold_PC != 0)
        return hold_PC;

    return 0;
}

template <typename ISA>
void Fetch<ISA>::clock( Cycle cycle)
{
    /* Process BP updates */
    if ( rp_memory_2_bp->is_ready( cycle))
        bp->update( rp_memory_2_bp->read( cycle));

    /* getting PC */
    auto PC = get_PC( cycle);

    /* hold PC for the stall case */
    wp_hold_pc->write( PC, cycle);

    /* ignore bubbles */
    if( PC == 0)
        return;

    Instr instr( memory->fetch_instr( PC), bp->get_bp_info( PC));

    /* updating PC according to prediction */
    wp_target->write( instr.get_predicted_target(), cycle);

    /* sending to decode */
    wp_fetch_2_decode->write( instr, cycle);

    /* log */
    sout << "fetch   cycle " << std::dec << cycle << ": 0x"
         << std::hex << PC << ": 0x" << instr << std::endl;
}

#include <mips/mips.h>

template class Fetch<MIPS>;
