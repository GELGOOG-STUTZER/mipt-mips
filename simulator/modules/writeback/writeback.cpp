#include "writeback.h"

#include <cassert>
#include <sstream>

template <typename ISA>
Writeback<ISA>::Writeback( Endian endian, bool log) : Log( log), endian( endian)
{
    rp_mem_datapath = make_read_port<Instr>("MEMORY_2_WRITEBACK", PORT_LATENCY);
    rp_execute_datapath = make_read_port<Instr>("EXECUTE_2_WRITEBACK", PORT_LATENCY);
    rp_branch_datapath = make_read_port<Instr>("BRANCH_2_WRITEBACK", PORT_LATENCY);
    rp_trap = make_read_port<bool>("WRITEBACK_2_ALL_FLUSH", PORT_LATENCY);

    wp_bypass = make_write_port<std::pair<RegisterUInt, RegisterUInt>>("WRITEBACK_2_EXECUTE_BYPASS", PORT_BW);
    wp_halt = make_write_port<bool>("WRITEBACK_2_CORE_HALT", PORT_BW);
    wp_trap = make_write_port<bool>("WRITEBACK_2_ALL_FLUSH", PORT_BW);
    wp_target = make_write_port<Target>("WRITEBACK_2_FETCH_TARGET", PORT_BW);
}

template<typename ISA>
void Writeback<ISA>::set_target( const Target& value, Cycle cycle)
{
    checker.set_target( value);
    next_PC = value.address;
    wp_trap->write( true, cycle);
    wp_target->write( value, cycle);
}
    
template <typename ISA>
auto Writeback<ISA>::read_instructions( Cycle cycle)
{
    auto ports = { rp_branch_datapath.get(), rp_mem_datapath.get(), rp_execute_datapath.get() };
    std::vector<Instr> result;

    for ( auto& port : ports)
        if ( port->is_ready( cycle))
            result.emplace_back( port->read( cycle));

    return result;
}

template <typename ISA>
void Writeback<ISA>::clock( Cycle cycle)
{
    sout << "wb      cycle " << std::dec << cycle << ": ";
    if ( rp_trap->is_ready( cycle) && rp_trap->read( cycle)) {
        writeback_bubble( cycle);
        return;
    }

    auto instrs = read_instructions( cycle);

    if ( instrs.empty())
        writeback_bubble( cycle);
    else for ( const auto& i : instrs)
        writeback_instruction( i, cycle);
}

template <typename ISA>
void Writeback<ISA>::writeback_bubble( Cycle cycle)
{
    sout << "bubble\n";
    if ( cycle >= last_writeback_cycle + 100_lt)
        throw Deadlock( "");
}

template <typename ISA>
void Writeback<ISA>::writeback_instruction( const Writeback<ISA>::Instr& instr, Cycle cycle)
{
    rf->write_dst( instr);
    wp_bypass->write( std::make_pair(instr.get_v_dst(), instr.get_v_dst2()), cycle);

    sout << instr << std::endl;

    checker.check( instr);
    ++executed_instrs;
    last_writeback_cycle = cycle;
    next_PC = instr.get_actual_target().address;
    if ( executed_instrs >= instrs_to_run || instr.is_halt())
        wp_halt->write( true, cycle);

    if ( instr.has_trap())
        set_target( instr.get_actual_target(), cycle);
}

template <typename ISA>
int Writeback<ISA>::get_exit_code() const noexcept
{
    return 0;
}

#include <mips/mips.h>
#include <risc_v/risc_v.h>

template class Writeback<MIPSI>;
template class Writeback<MIPSII>;
template class Writeback<MIPSIII>;
template class Writeback<MIPSIV>;
template class Writeback<MIPS32>;
template class Writeback<MIPS64>;
template class Writeback<MARS>;
template class Writeback<MARS64>;
template class Writeback<RISCV32>;
template class Writeback<RISCV64>;
template class Writeback<RISCV128>;
