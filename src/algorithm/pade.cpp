/*****************************************************************************
 *   Copyright (C) 2004-2013 The PaGMO development team,                     *
 *   Advanced Concepts Team (ACT), European Space Agency (ESA)               *
 *   http://apps.sourceforge.net/mediawiki/pagmo                             *
 *   http://apps.sourceforge.net/mediawiki/pagmo/index.php?title=Developers  *
 *   http://apps.sourceforge.net/mediawiki/pagmo/index.php?title=Credits     *
 *   act@esa.int                                                             *
 *                                                                           *
 *   This program is free software; you can redistribute it and/or modify    *
 *   it under the terms of the GNU General Public License as published by    *
 *   the Free Software Foundation; either version 2 of the License, or       *
 *   (at your option) any later version.                                     *
 *                                                                           *
 *   This program is distributed in the hope that it will be useful,         *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of          *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the           *
 *   GNU General Public License for more details.                            *
 *                                                                           *
 *   You should have received a copy of the GNU General Public License       *
 *   along with this program; if not, write to the                           *
 *   Free Software Foundation, Inc.,                                         *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.               *
 *****************************************************************************/


#include <string>
#include <vector>
#include <algorithm>

#include "../exceptions.h"
#include "../population.h"
#include "../problem/base.h"
#include "../archipelago.h"
#include "../island.h"
#include "../population.h"
#include "../topology/unconnected.h"
#include "../problem/decompose.h"
#include "../types.h"
#include "base.h"
#include "pade.h"

namespace pagmo { namespace algorithm {
/// Constructor
 /**
 * Constructs a PaDe algorithm
 *
 * @param[in] gen Number of generations to evolve.
 * @param[in] max_parallelism limits the amounts of threads spawned
 * @param[in] method the decomposition method to use (Weighted, Tchebycheff or BI)
 * @param[in] solver the algorithm to solve the single objective problems.
 *
 * @throws value_error if gen is negative
 * @see pagmo::problem::decompose::method_type
 */
pade::pade(int gen, unsigned int max_parallelism, pagmo::problem::decompose::method_type method, const pagmo::algorithm::base & solver):base(),m_gen(gen),m_max_parallelism(max_parallelism),m_method(method),m_solver(solver.clone())
{
	if (gen < 0) {
		pagmo_throw(value_error,"number of generations must be nonnegative");
	}
}

/// Copy constructor. Performs a deep copy.
pade::pade(const pade &algo):base(algo), m_gen(algo.m_gen),m_max_parallelism(algo.m_max_parallelism),m_method(algo.m_method),m_solver(algo.m_solver->clone())
{}

/// Clone method.
base_ptr pade::clone() const
{
	return base_ptr(new pade(*this));
}

//Recursive function building all m-ple of elements of X summing to s
void pade::reksum(std::vector<std::vector<double> > &retval, 
	       const std::vector<unsigned int>& X, 
	       unsigned int m, 
	       unsigned int s, 
	       std::vector<double> eggs) const {
	
	if (m==1) {
		if (std::find(X.begin(),X.end(),s) == X.end()) { //not found
			return;
		} else {
			eggs.push_back(s);
			retval.push_back(eggs);
		}
	} else {
		for (unsigned int i=0; i<X.size(); ++i) {
			eggs.push_back(X[i]);
			reksum(retval,X,m-1,s-X[i],eggs);
			eggs.pop_back();
		}
	}
}


/// Evolve implementation.
/**
 * Run the PaDe algorithm for the number of generations specified in the constructors.
 *
 * @param[in,out] pop input/output pagmo::population to be evolved.
 */

void pade::evolve(population &pop) const
{
	// Let's store some useful variables.
	const problem::base &prob = pop.problem();
	const population::size_type NP = pop.size();
	
	if ( prob.get_f_dimension() < 2 ) {
		pagmo_throw(value_error, "The problem is not multiobjective, try some other algorithm than PaDE");
	}

	// Get out if there is nothing to do.
	if (m_gen == 0) {
		return;
	}

	decision_vector dummy(pop.get_individual(0).cur_x.size(),0); //used for initialisation purposes
	std::vector<decision_vector> X(NP,dummy); //set of population chromosomes


	// Copy the population chromosomes into X
	for ( population::size_type i = 0; i<NP; i++ ) {
		X[i]	=	pop.get_individual(i).cur_x;
	}
	//clear the current population
	pop.clear();
	
	// generate the weights
	const unsigned int H = 21;
	std::vector<unsigned int> range;
	for (unsigned int i=0; i<H;++i) {
		range.push_back(i);
	}
	std::vector<fitness_vector> weights;
	reksum(weights, range, prob.get_f_dimension(), H-1);
	for(unsigned int i=0; i< weights.size(); ++i) {
		for(unsigned int j=0; j< weights[i].size(); ++j) {
			weights[i][j] /= (H-1);
		}
	}

	unsigned int count = 0;
	for(population::size_type p = 0; p < NP /m_max_parallelism + 1; p++) {
		pagmo::archipelago arch;
		arch.set_topology(topology::unconnected());

		//Each island in the archipelago solve a different single-objective problem
		for(pagmo::population::size_type i=0; i<m_max_parallelism && p*m_max_parallelism + i < NP;++i) {
			pagmo::problem::decompose decomposed_prob(prob, m_method,weights[count++]);
			pagmo::population decomposed_pop(decomposed_prob);

			//Set the individuals of the new population
			for(pagmo::population::size_type j=0; j<NP;++j) {
				decomposed_pop.push_back(X[j]);
			}
			arch.push_back(pagmo::island(*m_solver,decomposed_pop));
		}
		arch.evolve(m_gen);
		arch.join();


		//The population is set to contain the best individual of each island
		for(pagmo::population::size_type i=0; i<m_max_parallelism && p*m_max_parallelism + i < NP;++i) {
			pop.push_back(arch.get_island(i)->get_population().champion().x);
		}
	}
}

/// Algorithm name
std::string pade::get_name() const
{
	return "Parallel Decomposition (PaDe)";
}

/// Extra human readable algorithm info.
/**
 * Will return a formatted string displaying the parameters of the algorithm.
 */
std::string pade::human_readable_extra() const
{
	std::ostringstream s;
	s << "gen:" << m_gen << ' ';
	s << "max_parallelism:" << m_max_parallelism << ' ';
	s << "method:" << m_method << ' ';
	s << "solver:" << m_solver << ' ';
	return s.str();
}

}} //namespaces

BOOST_CLASS_EXPORT_IMPLEMENT(pagmo::algorithm::pade);
