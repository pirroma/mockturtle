/*!
  \file aig_algebraic_rewriting.hpp
  \brief AIG algebraric rewriting

  EPFL CS-472 2021 Final Project Option 1
*/

#pragma once

#include "../networks/aig.hpp"
#include "../views/depth_view.hpp"
#include "../views/topo_view.hpp"

namespace mockturtle
{

namespace detail
{

template<class Ntk>
class aig_algebraic_rewriting_impl
{
  using node = typename Ntk::node;
  using signal = typename Ntk::signal;

public:
  aig_algebraic_rewriting_impl( Ntk& ntk )
    : ntk( ntk )
  {
    static_assert( has_level_v<Ntk>, "Ntk does not implement depth interface." );
  }

  void run()
  {
    bool cont{true}; /* continue trying */
    while ( cont )
    {
      cont = false; /* break the loop if no updates can be made */
      ntk.foreach_gate( [&]( node n ){
        if ( try_algebraic_rules( n ) )
        {
          ntk.update_levels();
          cont = true;
        }
      });
    }
  }

private:
  /* Try various algebraic rules on node n. Return true if the network is updated. */
  bool try_algebraic_rules( node n )
  {
    if ( try_associativity( n ) )
      return true;
    if ( try_distributivity( n ) )
      return true;
    /* TODO: add more rules here... */
    if (try_three_level_distributivity(n))
        return true;
    return false;
  }

  /* Try the associativity rule on node n. Return true if the network is updated. */
  bool try_associativity(node n)
  {
      /* TODO */
      signal critical_point, non_critical_first, non_critical_second; // signal to take into account which of them are critical
      bool is_critical = false;
      uint8_t level_NCP = 0; //level of the non critical gate 
      uint8_t level_CR = 0; //level of the critical gate
      uint32_t counter_critical = 0;
      uint32_t counter_non_critical = 0;
      
      if (!ntk.is_on_critical_path(n)) //if not critical path, useless to check if associativity applicable
          return false;
      else
      {
          if (ntk.level(n) != 1)       //no first level, if first level no optimization possible
          {
              //iterate on both the input of the n gate
              ntk.foreach_fanin(n, [&](signal child)
              {
                      if (!ntk.is_complemented(child) && ntk.is_on_critical_path(ntk.get_node(child)))
                      {
                          //iterate on the child of the n gate child (grandchild)
                          ntk.foreach_fanin(ntk.get_node(child), [&](signal signal_extr)
                          {
                                  if (ntk.is_on_critical_path(ntk.get_node(signal_extr)))

                                  {
                                      //save the critical signal (on the critical path) and increase the number of critical signal detected
                                      critical_point = signal_extr;
                                      counter_critical++;

                                      is_critical = true;
                                      level_CR = ntk.level(ntk.get_node(signal_extr)); //save the level of the gate on the critical path 
                                  }
                                  else
                                  {
                                      //increase number of non critical signal detected
                                      counter_non_critical++;
                                      non_critical_first = signal_extr;
                                  }
                          });
                      }
                      else
                      {
                          //if child of n-gate non critical, increase the counter and save it
                          counter_non_critical++;
                          non_critical_second = child;
                          level_NCP = ntk.level(ntk.get_node(child));
                      }
              });
          }

      }

     //make optimization if a critical path is detectd, if only one signal is critical and two are non critical
     if (is_critical == true && counter_critical == 1 && counter_non_critical == 2)

     {
        //optimization make sense only if the level of the critical gate is bigger than the level of the non critical one
        if (level_CR > level_NCP)
        {
           //substitution algorithm 
           signal new_node;
           new_node = ntk.create_and(critical_point, ntk.create_and(non_critical_first, non_critical_second));
           ntk.substitute_node(n, new_node);

           return true;
        }
     }

     return false;
  }

  /* Try the distributivity rule on node n. Return true if the network is updated. */
  bool try_distributivity(node n)
  {
    /* TODO */

    //series of bool to indicate feasibility in different level 
    bool possible = false, possible_child = false, possible_child_1 = false;

    uint32_t level_cur;
    uint32_t counter_prim_in = 0, counter_child = 0;    //counter of the PI, depending on it the optimization could be useless
                                                        //count of the child, to know if both on the critical path 

    signal critical_vector[2];                          //overall 4 child, to be optimized, two shold be on the critical path and
    signal NC_vector[2];                                //should be the same signal, two not on the critical path 

    if (!ntk.is_on_critical_path(n) || ntk.level(n) == 1) //if n gate non critical and if level of the gate = 1, no optimization
        return false;

    //iterate on n-gate child
    ntk.foreach_fanin(n, [&](signal const& signal_extr)
    {
        if (!ntk.is_complemented(signal_extr))
            return;     //if not complemented, no optimization, return and exit with false
               
        possible = true;    

        node child = ntk.get_node(signal_extr);     //save the child of n-gate temporary, for easier computation

        level_cur = ntk.level(child);               //save the current level of the child

        if (level_cur == 1)                         //if level equal to 1, need to increase the primary input counter
            counter_prim_in++;

        //if the number of the primary input is equal to the number of n-gate fanin the optimization is not advantageous
        if (counter_prim_in != ntk.fanin_size(n))
        {
            if (ntk.is_on_critical_path(child))
            {
                counter_child++;                                            //if the child is on CP (critical path) increment of the counter
                //iterate on the child of n-gate child
                ntk.foreach_fanin(child, [&](signal const& signal_child)
                    {
                        node granchild = ntk.get_node(signal_child);
                                          
                        if (!ntk.is_on_critical_path(granchild) && counter_child == 1)
                        {
                            //if not critical, save it on the non-critical vector
                            possible_child = true;
                            NC_vector[0] = signal_child;
                        }
                        else if (ntk.is_on_critical_path(granchild) && counter_child == 1)
                            critical_vector[0] = signal_child;              //if critical update the right vector

                        else if (!ntk.is_on_critical_path(granchild))
                        {
                            possible_child_1 = true;
                            NC_vector[1] = signal_child;
                        }
                        else
                            critical_vector[1] = signal_child;
                        return;
                    });
            }
        }

        return;
        });
   if (critical_vector[0] != critical_vector[1])                //if the two final signal on the critical vector are different
      return false;                                             //no optimization is possible, no common signal, and the common
                                                                //signal should be on the critical path

    if (counter_prim_in != ntk.fanin_size(n) && possible && possible_child_1 && possible_child)
    {
        //algorithm to create the new logic
        signal or_logic = ntk.create_or(NC_vector[0], NC_vector[1]);
        signal out_gate;
        if (ntk.is_or(n))
            out_gate = ntk.create_and(or_logic, critical_vector[0]);
        else
            out_gate = ntk.create_nand(or_logic, critical_vector[0]);

        ntk.substitute_node(n, out_gate);
        return true;
    }

    return false;
  }


  bool try_three_level_distributivity(node n)
  {
      signal critical_zero, critical_first, critical_second;
      signal NC_zero, NC_first, NC_second;
      uint8_t level_safe;
      uint8_t level_crit;

      //flag to take into account all the critical and non critical gate at each level 
      bool first_critical = false; 
      bool first_child = false;
      bool second_critical = false;
      bool second_child = false;
      bool third_child = false;
      bool third_critical = false;

      //if the gate is not on critial path, no optimization
      if (!ntk.is_on_critical_path(n))
          return false;

      //iteration of the n-gate child
      ntk.foreach_fanin(n, [&](signal child)
      {
              //if on critical and complemented, set bool, save signal and save level
              if (ntk.is_on_critical_path(ntk.get_node(child)) && ntk.is_complemented(child))
              {
                  first_critical = true;
                  critical_zero = child;
                  level_crit = ntk.level(ntk.get_node(critical_zero));
              }
              //if not on critical, sset bool, save signal, and level
              else if (!ntk.is_on_critical_path(ntk.get_node(child)))
              {
                  first_child = true;
                  NC_zero = child;
                  level_safe = ntk.level(ntk.get_node(NC_zero));
              }
              
      });
      //check on the level, if the critical level is close (by 2) to the low level, the optimization is useless
      if (level_crit <= 2 + level_safe)
          return false;
      //check on the previous two bool
      if (first_critical && first_child)
      {
          //same previous approach
          ntk.foreach_fanin(ntk.get_node(critical_zero), [&](signal granchild)
              {
                  if (ntk.is_on_critical_path(ntk.get_node(granchild)) && ntk.is_complemented(granchild))
                  {
                      second_critical = true;
                      critical_first = granchild;
                  }
                  else if (ntk.is_complemented(granchild))
                  {
                      second_child = true;
                      NC_first = granchild;
                  }
              });
      }

      //check on all the previous bool
      if (first_critical && first_child && second_critical && second_child)
      { 
          //same method of previously, but no worry about the complementation
          ntk.foreach_fanin(ntk.get_node(critical_first), [&](signal grangranchild)
              {
                  if (ntk.is_on_critical_path(ntk.get_node(grangranchild)))
                  {
                      third_critical = true;
                      critical_second = grangranchild;
                  }
                  else
                  {
                      third_child = true;
                      NC_second = grangranchild;
                  }
              });
      }

      //if all the condition were fulliflled, and the structure can be optimized
      if (first_critical && first_child && second_critical && second_child && third_child && third_critical)
      {
              //creation of all the gates
              signal and_new_0;
              and_new_0 = ntk.create_and(NC_second, NC_zero);
              signal and_new_1;
              and_new_1 = ntk.create_and(critical_second, and_new_0);
              signal and_new_2;
              and_new_2 = ntk.create_and(NC_zero, ntk.create_not(NC_first));
              signal nand_output;
              nand_output = ntk.create_nand(ntk.create_not(and_new_1), ntk.create_not(and_new_2));

              ntk.substitute_node(n, nand_output);
              return true;
      }
      return false;
  }




private:
  Ntk& ntk;
};

} // namespace detail

/* Entry point for users to call */
template<class Ntk>
void aig_algebraic_rewriting( Ntk& ntk )
{
  static_assert( std::is_same_v<typename Ntk::base_type, aig_network>, "Ntk is not an AIG" );

  depth_view dntk{ntk};
  detail::aig_algebraic_rewriting_impl p( dntk );
  p.run();
}

} /* namespace mockturtle */
