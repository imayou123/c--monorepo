#include <fstream>
#include <random>

#include <gflags/gflags.h>

#include <minimum/core/color.h>
#include <minimum/core/grid.h>
#include <minimum/core/main.h>
#include <minimum/core/random.h>
#include <minimum/core/range.h>
#include <minimum/linear/colgen/set_partitioning_problem.h>
#include <minimum/linear/retail_scheduling.h>

using namespace std;
using namespace minimum::core;
using namespace minimum::linear;
using namespace minimum::linear::colgen;

DEFINE_int32(num_solutions, 1, "Number of solutions to compute. Default: 1.");

bool create_roster(const RetailProblem& problem,
                   const vector<double>& duals,
                   int staff_index,
                   const vector<vector<int>>& fixes,
                   vector<vector<int>>* solution,
                   mt19937* rng) {
	uniform_real_distribution<double> rand(0, 1);

	// TODO: Implement this method with something reasonable.

	for (auto d : range(problem.periods.size())) {
		for (auto s : range(problem.num_tasks)) {
			if (fixes[d][s] >= 0) {
				(*solution)[d][s] = fixes[d][s];
				continue;
			}

			int c = problem.cover_constraint_index(d, s);
			int row = problem.staff.size() + c;
			if (duals.at(row) > 0) {
				(*solution)[d][s] = 1;
			} else {
				(*solution)[d][s] = 0;
			}
		}
	}

	return true;
}

class ShiftShedulingColgenProblem : public SetPartitioningProblem {
   public:
	ShiftShedulingColgenProblem(RetailProblem problem_)
	    : SetPartitioningProblem(problem_.staff.size(), problem_.num_cover_constraints()),
	      problem(move(problem_)) {
		for (int p : range(problem.staff.size())) {
			rng.emplace_back(repeatably_seeded_engine<std::mt19937>(p));
		}

		Timer timer("Setting up colgen problem");
		solution = make_grid<int>(problem.staff.size(), problem.periods.size(), problem.num_tasks);

		// Covering constraints with slack variables.
		for (auto d : range(problem.periods.size())) {
			auto& period = problem.periods[d];
			for (auto s : range(problem.num_tasks)) {
				int c = problem.cover_constraint_index(d, s);
				// Initialize this constraint.
				initialize_constraint(
				    c, period.min_cover.at(s), period.max_cover.at(s), 1000.0, 1000.0);
			}
		}
		timer.OK();

		timer.next("Creating initial rosters");

		// Initial duals just try to cover as much as possible, with some
		// randomness to encourage different rosters.
		auto initial_duals = make_grid<double>(problem.staff.size(), number_of_rows());
		uniform_real_distribution<double> dual(1, 100);
		for (int p = 0; p < problem.staff.size(); ++p) {
			for (int r = 0; r < number_of_rows(); ++r) {
				initial_duals[p][r] = dual(rng[p]);
			}
		}

		auto no_fixes =
		    make_grid<int>(problem.periods.size(), problem.num_tasks, []() { return -1; });

		for (int p = 0; p < problem.staff.size(); ++p) {
			minimum_core_assert(
			    create_roster(problem, initial_duals[p], p, no_fixes, &solution[p], &rng[p]));
		}

		for (int p = 0; p < problem.staff.size(); ++p) {
			add_column(create_column(p, solution[p]));
		}

		timer.OK();
	}

	~ShiftShedulingColgenProblem() {}

	Column create_column(int staff_index, const vector<vector<int>>& solution_for_staff) {
		double cost = 0;

		// TODO: Set the cost correctly.

		Column column(cost, 0, 1);
		column.add_coefficient(staff_index, 1);

		int start_row = problem.staff.size();
		for (auto d : range(problem.periods.size())) {
			for (auto s : range(problem.num_tasks)) {
				int c = problem.cover_constraint_index(d, s);
				int sol = solution_for_staff[d][s];
				minimum_core_assert(sol == 0 || sol == 1);
				if (sol == 1) {
					column.add_coefficient(start_row + c, 1.0);
				}
			}
		}
		return column;
	}

	virtual void generate(const std::vector<double>& dual_variables) override {
		// vector of ints instead of bool because we may be writing to
		// it concurrently.
		vector<int> success(problem.staff.size(), 0);

		for (int p = 0; p < problem.staff.size(); ++p) {
			if (generate_for_staff(p, dual_variables)) {
				success[p] = 1;
			}
		}

		for (int p = 0; p < problem.staff.size(); ++p) {
			if (success[p] == 1) {
				add_column(create_column(p, solution[p]));
			}
		}
	}

	virtual std::string member_name(int member) const override {
		return problem.staff.at(member).id;
	}

   private:
	bool generate_for_staff(int staff_index, const std::vector<double>& dual_variables) {
		if (member_fully_fixed(staff_index)) {
			return false;
		}

		auto fixes = make_grid<int>(problem.periods.size(), problem.num_tasks);
		auto& fixes_for_staff = fixes_for_member(staff_index);
		for (auto c : range(problem.num_cover_constraints())) {
			int d = problem.cover_constraint_to_period(c);
			int s = problem.cover_constraint_to_task(c);
			fixes[d][s] = fixes_for_staff[c];
		}

		return create_roster(
		    problem, dual_variables, staff_index, fixes, &solution[staff_index], &rng[staff_index]);
	}

	const minimum::linear::RetailProblem problem;
	std::vector<std::vector<std::vector<int>>> solution;
	vector<mt19937> rng;
};

int main_program(int num_args, char* args[]) {
	if (num_args <= 1) {
		cerr << "Need a file name.\n";
		return 1;
	}
	ifstream input(args[1]);
	const RetailProblem problem(input);
	problem.print_info();

	ShiftShedulingColgenProblem colgen_problem(problem);

	auto start_time = wall_time();
	for (int i = 1; i <= FLAGS_num_solutions; ++i) {
		colgen_problem.unfix_all();
		colgen_problem.solve();
		cerr << "Colgen done.\n";
		auto elapsed_time = wall_time() - start_time;
		cerr << "Elapsed time : " << elapsed_time << "s.\n";
	}

	return 0;
}

int main(int num_args, char* args[]) { return main_runner(main_program, num_args, args); }
