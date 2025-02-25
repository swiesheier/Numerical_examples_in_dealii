#include <deal.II/grid/tria.h>
#include <deal.II/grid/tria_accessor.h>
#include <deal.II/grid/tria_iterator.h>
#include <deal.II/grid/grid_generator.h>
#include <deal.II/grid/grid_out.h>
#include <deal.II/grid/manifold_lib.h>

#include "../MA-Code/enumerator_list.h"

#include <iostream>
#include <fstream>
#include <cmath>

using namespace dealii;

namespace QuarterHyperCube_Merged
/*
 * A quarter of a plate with hole in 2D or 1/8 in 3D
 *
 * CERTIFIED TO STANDARD numExS07 (200724)
 */
{
	// Name of the numerical example
	 std::string numEx_name = "QPlate";

	// The loading direction: \n
	// In which coordinate direction the load shall be applied, so x/y/z.
	 const unsigned int loading_direction = enums::y;

	// The loaded faces:
	 const enums::enum_boundary_ids id_boundary_load = enums::id_boundary_yPlus;
	 const enums::enum_boundary_ids id_boundary_secondaryLoad = enums::id_boundary_xPlus;

	 // QPlate
	  const enums::enum_BC BC_xMinus = enums::BC_sym;
	  const enums::enum_BC BC_xPlus = enums::BC_none;

	// Evaluation point
	 Point<3> eval_point;

	 // DENP
//	  const enums::enum_BC BC_xMinus = enums::BC_none;
//	  const enums::enum_BC BC_xPlus = enums::BC_sym;

	 const bool apply_sym_constraint_on_top_face = false;

	// Some internal parameters
	 struct parameterCollection
	 {
		const types::boundary_id boundary_id_hole = 10;
		const types::manifold_id manifold_id_hole = 10;

		const double search_tolerance = 1e-12;
	 };

	template<int dim>
	void make_constraints ( AffineConstraints<double>  &constraints, const FESystem<dim> &fe, unsigned int &n_components, DoFHandler<dim> &dof_handler_ref,
							const bool &apply_dirichlet_bc, const double &current_load_increment,
							const Parameter::GeneralParameters &parameter )
	{
		/* inputs:
		 * dof_handler_ref,
		 * fe
		 * apply_dirichlet_bc
		 * constraints
		 * current_load_increment
		 */

		const FEValuesExtractors::Vector displacement(0);
		const FEValuesExtractors::Scalar x_displacement(0);
		const FEValuesExtractors::Scalar y_displacement(1);

		// Symmetry constraints:
		// Update and apply new constraints
		//		on x0_plane for symmetry (displacement_in_x = 0)
		//		on y0_plane for symmetry (displacement_in_y = 0)

		// BC on x0 plane
		 if ( BC_xMinus==enums::BC_sym )
			 numEx::BC_apply( enums::id_boundary_xMinus, enums::x, 0, apply_dirichlet_bc, dof_handler_ref, fe, constraints );

		 if ( BC_xPlus==enums::BC_sym )
			 numEx::BC_apply( enums::id_boundary_xPlus, enums::x, 0, apply_dirichlet_bc, dof_handler_ref, fe, constraints );

		// on bottom edge
		 numEx::BC_apply( enums::id_boundary_yMinus, enums::y, 0, apply_dirichlet_bc, dof_handler_ref, fe, constraints );

		// BC on z0 plane ...
		 if ( dim==3 ) // ... only for 3D
		 {
			// For compression we don't fix anything in the third direction, because y0 was already clamped.
			// @todo However, what about the upper part?
			 numEx::BC_apply( enums::id_boundary_zMinus, enums::z, 0, apply_dirichlet_bc, dof_handler_ref, fe, constraints );

			if ( apply_sym_constraint_on_top_face )
				numEx::BC_apply( enums::id_boundary_zPlus, enums::z, 0, apply_dirichlet_bc, dof_handler_ref, fe, constraints );
		 }

		// BC for the load ...
		 if ( parameter.driver == enums::Dirichlet )  // ... as Dirichlet only for Dirichlet as driver, alternatively  ...
			numEx::BC_apply( id_boundary_load, loading_direction, current_load_increment, apply_dirichlet_bc, dof_handler_ref, fe, constraints );
	}

	// ToDo-optimize: use existing DII command	void GridGenerator::plate_with_a_hole

	// to see the effects of the inputs (lengths, refinements, etc) consider using the output (.eps, etc) below
	void make_2d_quarter_plate_with_hole( Triangulation<2> &tria_2d,
										  const double half_length,
										  const double half_width,
										  const double hole_radius,
										  const double hole_division_fraction, const Parameter::GeneralParameters &parameter )
	{
		const double length = 2.0*half_length;
		const double width =  2.0*half_width;
		const double hole_diameter = 2.0*hole_radius;
		const double internal_width = hole_diameter + hole_division_fraction*(width - hole_diameter);

		// Set the evaluation point in the top right corner
		 eval_point[enums::x] = half_width;
		 eval_point[enums::y] = half_length;

		Triangulation<2> tria_quarter_plate_hole;
		{
			Triangulation<2> tria_plate_hole;
			GridGenerator::hyper_cube_with_cylindrical_hole ( tria_plate_hole,
															  hole_diameter/2.0,
															  internal_width/2.0 );

			std::set<typename Triangulation<2>::active_cell_iterator > cells_to_remove;
			for (typename Triangulation<2>::active_cell_iterator
				 cell = tria_plate_hole.begin_active();
				 cell != tria_plate_hole.end(); ++cell)
			{
				// Remove all cells that are not in the first quadrant
				if (cell->center()[0] < 0.0 || cell->center()[1] < 0.0)
					cells_to_remove.insert(cell);
			}

			Assert(cells_to_remove.size() > 0, ExcInternalError());
			Assert(cells_to_remove.size() != tria_plate_hole.n_active_cells(), ExcInternalError());
			GridGenerator::create_triangulation_with_removed_cells(tria_plate_hole,cells_to_remove,tria_quarter_plate_hole);
		}

		// If we chose a fraction of 1, we do not add the outer plate to it, ...
		if ( hole_division_fraction==1. )
		{
			Triangulation<2> tria_cut_plate;
			{
				Triangulation<2> tria_plate;

				// Subdivide the plate so that we're left one cell to remove (we'll replace this with the plate with the hole)
				// and then make the rest of the subdivisions so that we're left with cells with a decent aspect ratio
				std::vector<std::vector<double> > step_sizes;

				GridGenerator::hyper_rectangle( tria_plate,
												Point<2>(0.0, 0.0),
												Point<2>(width, length) );

				tria_plate.refine_global( 1 );

				std::set<typename Triangulation<2>::active_cell_iterator > cells_to_remove;
				for (typename Triangulation<2>::active_cell_iterator
					 cell = tria_plate.begin_active();
					 cell != tria_plate.end(); ++cell)
				{
					// Remove all cells that are in the bottom left corner, so where the small_plate_with_hole will be entered
					if (cell->center()[0] < width/2. && cell->center()[1] < width/2. )
					{
						cells_to_remove.insert(cell);
					}
				}

				Assert(cells_to_remove.size() > 0, ExcInternalError());
				Assert(cells_to_remove.size() != tria_plate.n_active_cells(), ExcInternalError());
				GridGenerator::create_triangulation_with_removed_cells(tria_plate,cells_to_remove,tria_cut_plate);
			}

			Triangulation<2> tria_2d_not_flat;
			GridGenerator::merge_triangulations( tria_quarter_plate_hole,
												 tria_cut_plate,
												 tria_2d_not_flat );

			// Attach a manifold to the curved boundary and refine
			// @note We can only guarantee that the vertices sit on the curve, so we must test with their position instead of the cell centre.
			const Point<2> centre_2d (0,0);
			for (typename Triangulation<2>::active_cell_iterator
			   cell = tria_2d_not_flat.begin_active();
			   cell != tria_2d_not_flat.end(); ++cell)
			{
			  for (unsigned int face=0; face<GeometryInfo<2>::faces_per_cell; ++face)
				if (cell->face(face)->at_boundary())
				  for (unsigned int vertex=0; vertex<GeometryInfo<2>::vertices_per_face; ++vertex)
				  {
					if (std::abs(cell->vertex(vertex).distance(centre_2d) - hole_diameter/2.0) < 1e-12)
					 {
						cell->face(face)->set_manifold_id(10);
						break;
					 }
				  }
			}

			SphericalManifold<2> spherical_manifold_2d (centre_2d);
			tria_2d_not_flat.set_manifold(10,spherical_manifold_2d);

			if ( parameter.stepwise_global_refinement==true ) // for the special case of step by step global refinement ...
				tria_2d_not_flat.refine_global( 1 );	// ... only refine the initial grid once
			else	// for the standard case of AMR refine the grid as specified in the ...
				tria_2d_not_flat.refine_global(parameter.nbr_global_refinements);	// ...Parameter.prm file; has to be refined before the manifolds are deleted again

//			AssertThrow(parameter.nbr_holeEdge_refinements==0, ExcMessage("QuarterPlate mesh creation: Sorry, right now you cannot use hole edge refinements."));

			tria_2d_not_flat.reset_manifold(10); // Clear manifold

			GridGenerator::flatten_triangulation(tria_2d_not_flat,tria_2d);

			// Remove the dummy cells around the quarter plate
			std::set<typename Triangulation<2>::active_cell_iterator > cells_to_remove;
			for (typename Triangulation<2>::active_cell_iterator
				 cell = tria_2d.begin_active();
				 cell != tria_2d.end(); ++cell)
			{
				// Remove all cells that are not in the first quadrant
				if (cell->center()[0] > width/2. || cell->center()[1] > width/2. )
					cells_to_remove.insert(cell);
			}

			Assert(cells_to_remove.size() > 0, ExcInternalError());
			GridGenerator::create_triangulation_with_removed_cells(tria_2d,cells_to_remove,tria_2d);
		}
		// else we add a nice quad mesh of a plate around the quarter plate
		else
		{
			Triangulation<2> tria_cut_plate;
			{
				Triangulation<2> tria_plate;

				// Subdivide the plate so that we're left one cell to remove (we'll replace this with the plate with the hole)
				// and then make the rest of the subdivisions so that we're left with cells with a decent aspect ratio
				std::vector<std::vector<double> > step_sizes;
				// for width
				{
					std::vector<double> subdivision_width;
					subdivision_width.push_back(internal_width/2.0);
					const double width_remaining = (width - internal_width)/2.0;
					const unsigned int n_subs = std::max(1.0,std::ceil(width_remaining/(internal_width/2.0)));
					Assert(n_subs>0, ExcInternalError());
					for (unsigned int s=0; s<n_subs; ++s)
						subdivision_width.push_back(width_remaining/n_subs);
					step_sizes.push_back(subdivision_width);
					const double sum_half_width = std::accumulate(subdivision_width.begin(), subdivision_width.end(), 0.0);
					AssertThrow(std::abs(sum_half_width-width/2.0) < 1e-12, ExcInternalError());
				}
				// for length
				{
					std::vector<double> subdivision_length;
					subdivision_length.push_back(internal_width/2.0);
					const double length_remaining = (length - internal_width)/2.0;

					const unsigned int n_subs = std::max(1.0,std::ceil(length_remaining/(internal_width/2.0)));
					Assert(n_subs>0, ExcInternalError());
					for (unsigned int s=0; s<n_subs; ++s)
					subdivision_length.push_back(length_remaining/n_subs);
					step_sizes.push_back(subdivision_length);
					const double sum_half_length = std::accumulate(subdivision_length.begin(), subdivision_length.end(), 0.0);
					AssertThrow(std::abs(sum_half_length-length/2.0) < 1e-12, ExcInternalError());
				}

				GridGenerator::subdivided_hyper_rectangle( tria_plate,
														   step_sizes,
														   Point<2>(0.0, 0.0),
														   Point<2>(width/2.0, length/2.0) );

				std::set<typename Triangulation<2>::active_cell_iterator > cells_to_remove;
				for (typename Triangulation<2>::active_cell_iterator
					 cell = tria_plate.begin_active();
					 cell != tria_plate.end(); ++cell)
				{
					// Remove all cells that are in the bottom left corner, so where the small_plate_with_hole will be entered
					if (cell->center()[0] < internal_width/2.0 && cell->center()[1] < internal_width/2.0)
						cells_to_remove.insert(cell);
				}

				Assert(cells_to_remove.size() > 0, ExcInternalError());
				Assert(cells_to_remove.size() != tria_plate.n_active_cells(), ExcInternalError());
				GridGenerator::create_triangulation_with_removed_cells(tria_plate,cells_to_remove,tria_cut_plate);
			}

			Triangulation<2> tria_2d_not_flat;
			GridGenerator::merge_triangulations( tria_quarter_plate_hole,
												 tria_cut_plate,
												 tria_2d_not_flat );


			// Attach a manifold to the curved boundary and refine
			// @note We can only guarantee that the vertices sit on the curve, so we must test with their position instead of the cell centre.
			const Point<2> centre_2d (0,0);
			for (typename Triangulation<2>::active_cell_iterator
			   cell = tria_2d_not_flat.begin_active();
			   cell != tria_2d_not_flat.end(); ++cell)
			{
			  for (unsigned int face=0; face<GeometryInfo<2>::faces_per_cell; ++face)
				if (cell->face(face)->at_boundary())
				  for (unsigned int vertex=0; vertex<GeometryInfo<2>::vertices_per_face; ++vertex)
					if (std::abs(cell->vertex(vertex).distance(centre_2d) - hole_diameter/2.0) < 1e-12)
					 {
						cell->face(face)->set_manifold_id(10);
						break;
					 }
			}

			SphericalManifold<2> spherical_manifold_2d (centre_2d);
			tria_2d_not_flat.set_manifold(10,spherical_manifold_2d);

			if ( parameter.stepwise_global_refinement==true ) // for the special case of step by step global refinement ...
				tria_2d_not_flat.refine_global( 1 );	// ... only refine the initial grid once
			else	// for the standard case of AMR refine the grid as specified in the ...
				tria_2d_not_flat.refine_global(parameter.nbr_global_refinements);	// ...Parameter.prm file; has to be refined before the manifolds are deleted again

//			AssertThrow(parameter.nbr_holeEdge_refinements==0, ExcMessage("QuarterPlate mesh creation: Sorry, right now you cannot use hole edge refinements."));
			// The following won't work because the created hanging nodes are not saved, hence no constraints are put upon them
	//		// pre-refinement of the inner area (around the hole edge)
	//		 for (unsigned int refine_counter=0; refine_counter<parameter.nbr_holeEdge_refinements; refine_counter++)
	//		 {
	//			for (typename Triangulation<2>::active_cell_iterator
	//						 cell = tria_2d_not_flat.begin_active();
	//						 cell != tria_2d_not_flat.end(); ++cell)
	//			{
	//				double distance2D = std::sqrt( cell->center()[0]*cell->center()[0] + cell->center()[1]*cell->center()[1] );
	//
	//				for ( unsigned int face=0; face<GeometryInfo<2>::faces_per_cell; face++ )
	//					if ( distance2D < 2 || cell->face(face)->boundary_id() == parameters_internal.boundary_id_hole )	// either the cell center is within 2*hole_radius=2 or one face is at the hole edge
	//					{
	//						cell->set_refine_flag();
	//						break;
	//					}
	//			}
	//			tria_2d_not_flat.execute_coarsening_and_refinement();
	//		 }

			tria_2d_not_flat.reset_manifold(10); // Clear manifold

			GridGenerator::flatten_triangulation(tria_2d_not_flat,tria_2d);
		}

		// include the following scope to see directly how the variation of the input parameters changes the geometry of the grid
		/*
		{
			std::ofstream out ("grid-tria_2d_not_flat.eps");
			GridOut grid_out;
			grid_out.write_eps (tria_2d_not_flat, out);
			std::cout << "Grid written to grid-tria_2d_not_flat.eps" << std::endl;
		}
		*/
	}

// 2D grid
	template <int dim>
	void make_grid( Triangulation<2> &triangulation, const Parameter::GeneralParameters &parameter )
	{
		parameterCollection parameters_internal;

		// size of the plate divided by the size of the hole
		  double ratio_width_To_holeRadius = parameter.width;
		  double width = parameter.width/2.;
		  double holeRadius = parameter.holeRadius;

		  // size of the inner mesh (hypercube with hole) relative to size of the whole plate
		  double ratio_x = parameter.ratio_x;

		  // USER parameter
		  const double refine_local_gradation = 0.75;

		const double search_tolerance = parameters_internal.search_tolerance;

		make_2d_quarter_plate_with_hole(
											triangulation,
											ratio_width_To_holeRadius,			// length
											ratio_width_To_holeRadius * 1.0,	// width: *1.0 => square
											holeRadius,	// hole radius = diameter/2
										    ratio_x, parameter
										);

		//Clear boundary ID's
		for ( typename Triangulation<dim>::active_cell_iterator
				cell = triangulation.begin_active();
					cell != triangulation.end(); ++cell )
		{
			for (unsigned int face=0; face<GeometryInfo<dim>::faces_per_cell; ++face)
			  if (cell->face(face)->at_boundary())
			  {
				  cell->face(face)->set_all_boundary_ids(0);
			  }
		}

		//Set boundary IDs and manifolds
		const Point<dim> centre (0,0);
		for ( typename Triangulation<dim>::active_cell_iterator
				cell = triangulation.begin_active();
					cell != triangulation.end(); ++cell )
		{
			for (unsigned int face=0; face<GeometryInfo<dim>::faces_per_cell; ++face)
			  if (cell->face(face)->at_boundary())
			  {
				//Set boundary IDs
				if (std::abs(cell->face(face)->center()[0] - 0.0) < search_tolerance)
				{
					cell->face(face)->set_boundary_id(enums::id_boundary_xMinus);	// the left edge
				}
				else if (std::abs(cell->face(face)->center()[0] - ratio_width_To_holeRadius) < search_tolerance)
				{
					cell->face(face)->set_boundary_id(enums::id_boundary_xPlus); // the right edge
				}
				else if (std::abs(cell->face(face)->center()[1] - 0.0) < search_tolerance)
				{
					cell->face(face)->set_boundary_id(enums::id_boundary_yMinus);	// the bottom edge

					for (unsigned int vertex=0; vertex<GeometryInfo<dim>::vertices_per_face; ++vertex)
					{
					 if (std::abs(cell->vertex(vertex)[enums::y] - 0) < search_tolerance)
					  if (std::abs(cell->vertex(vertex)[enums::x] - parameter.holeRadius) < search_tolerance)
					  {
						  // We found the cell that lies at the bottom edge next to the hole (bottom left corner)
						  cell->set_material_id( enums::tracked_QP );
						  break;
					  }
					}

				}
				else if (std::abs(cell->face(face)->center()[1] - ratio_width_To_holeRadius) < search_tolerance)
				{
					cell->face(face)->set_boundary_id(enums::id_boundary_yPlus); // the top edge
				}
				else
				{
					for (unsigned int vertex=0; vertex<GeometryInfo<dim>::vertices_per_face; ++vertex)
					  if (std::abs(cell->vertex(vertex).distance(centre) - parameter.holeRadius) < search_tolerance)
					  {
						  cell->face(face)->set_boundary_id(parameters_internal.boundary_id_hole);	// the hole edge
						  break;
					  }
				}

				//Set manifold IDs
				for (unsigned int vertex=0; vertex<GeometryInfo<dim>::vertices_per_face; ++vertex)
				  if (std::abs(cell->vertex(vertex).distance(centre) - parameter.holeRadius) < search_tolerance)
				  {
					  cell->face(face)->set_manifold_id(parameters_internal.manifold_id_hole);
					  break;
				  }
			  }
		}

		static SphericalManifold<dim> spherical_manifold (centre);
		triangulation.set_manifold(parameters_internal.manifold_id_hole,spherical_manifold);

//		if ( parameter.stepwise_global_refinement==true ) // for the special case of step by step global refinement ...
//			triangulation.refine_global( 1 );	// ... only refine the initial grid once
//		else	// for the standard case of AMR refine the grid as specified in the ...
//			triangulation.refine_global(parameter.nbr_global_refinements);	// ... Parameter.prm file
//
//		//GridTools::scale(parameters_internal.scale,triangulation);	// not sensible to use in this context
//
//		// pre-refinement of the inner area (around the hole edge)
//		for (unsigned int refine_counter=0; refine_counter<parameter.nbr_holeEdge_refinements; refine_counter++)
//		{
//			for (typename Triangulation<dim>::active_cell_iterator
//						 cell = triangulation.begin_active();
//						 cell != triangulation.end(); ++cell)
//			{
//				double distance2D = std::sqrt( cell->center()[0]*cell->center()[0] + cell->center()[1]*cell->center()[1] );
//
//				for ( unsigned int face=0; face<GeometryInfo<dim>::faces_per_cell; face++ )
//				{
//					if ( distance2D < 2 || cell->face(face)->boundary_id() == parameters_internal.boundary_id_hole )	// either the cell center is within 2*hole_radius=2 or one face is at the hole edge
//					{
//						cell->set_refine_flag();
//						break;
//					}
//				}
//			}
//			triangulation.execute_coarsening_and_refinement();
//		}

		// pre-refinement of the damaged area (around y=0)
		// One isotropic refinement ...
		if ( parameter.nbr_holeEdge_refinements > 0 )
		{
			for (typename Triangulation<dim>::active_cell_iterator
						 cell = triangulation.begin_active();
						 cell != triangulation.end(); ++cell)
			{
				for ( unsigned int vertex=0; vertex<GeometryInfo<dim>::vertices_per_cell; vertex++ )
				{
					if ( cell->vertex(vertex)[enums::y] < 30  )
					{
						cell->set_refine_flag();
						break;
					}
				}
			}
			triangulation.execute_coarsening_and_refinement();

			// ... the rest is anisotropic
			for (unsigned int refine_counter=0; refine_counter < parameter.nbr_holeEdge_refinements-1; refine_counter++)
			{
				for (typename Triangulation<dim>::active_cell_iterator
							 cell = triangulation.begin_active();
							 cell != triangulation.end(); ++cell)
				{
					for ( unsigned int face=0; face<GeometryInfo<dim>::faces_per_cell; face++ )
					{
						if ( cell->center()[enums::y] < 30 ) //width*0.75/(std::pow(double(refine_counter),refine_local_gradation)+1.) )
						{
							//cell->set_refine_flag();
							cell->set_refine_flag(RefinementCase<dim>::cut_x); // refine only in the y-direction
							break;
						}
					}
				}
	//			Point<dim> origin(50,0);
	//			for (typename Triangulation<dim>::active_cell_iterator
	//						 cell = triangulation.begin_active();
	//						 cell != triangulation.end(); ++cell)
	//			{
	//				for ( unsigned int vertex=0; vertex<GeometryInfo<dim>::vertices_per_cell; vertex++ )
	//				{
	//					//if ( std::abs( cell->vertex(vertex).distance(origin) -50 ) < 1e-5 && cell->vertex(vertex)[enums::y] < 25 )
	//					//if ( cell->vertex(vertex).distance(origin) < 1e-5 )
	//					if ( cell->vertex(vertex)[enums::y] < 10  )
	//					{
	//						//cell->set_refine_flag();
	//						cell->set_refine_flag(RefinementCase<dim>::cut_x); // refine only in the y-direction
	//						break;
	//					}
	////					else if ( std::abs( cell->vertex(vertex)[enums::x] - 100 ) < 1e-5 && cell->vertex(vertex)[enums::y] < 50 )
	////					{
	////						cell->set_refine_flag(RefinementCase<dim>::cut_x); // refine only in the y-direction
	////					}
	//				}
	//			}
				triangulation.execute_coarsening_and_refinement();
			}
		}
		// Output the triangulation as eps or inp
		 //numEx::output_triangulation( triangulation, enums::output_eps, numEx_name );
	}

/**
 * 3D Plate with a hole: 1/8 model
 */
	template <int dim>
	void make_grid( Triangulation<3> &triangulation, const Parameter::GeneralParameters &parameter )
	{
		parameterCollection parameters_internal;

		// size of the plate divided by the size of the hole
		 double ratio_width_To_holeRadius = parameter.width;
		 ///double width = parameter.width/2.;
		 double holeRadius = parameter.holeRadius;

		  // size of the inner mesh (hypercube with hole) relative to size of the whole plate
		  double ratio_x = parameter.ratio_x;

		const double search_tolerance = parameters_internal.search_tolerance;

		Triangulation<2> tria_2d;
		make_2d_quarter_plate_with_hole(
											tria_2d,
											ratio_width_To_holeRadius,			// length
											ratio_width_To_holeRadius * 1.0,	// width: *1.0 => square
											holeRadius,	// hole radius = diameter/2
											ratio_x, parameter //, parameters_internal
										);

		// only relevant for 3d grid:
		  const unsigned int n_repetitions_z = parameter.nbr_elementsInZ;			// nbr of Unterteilungen in z-direction for 3d meshing; 1=one element in z; 2=two el.s in z; ...

		GridGenerator::extrude_triangulation(tria_2d,
										   n_repetitions_z+1,
										   parameter.thickness/2.0,
										   triangulation);

		// Set the evaluation point's z-coordinate
		 eval_point[enums::z] = parameter.thickness/2.0;

		// Clear all existing boundary ID's
		 numEx::clear_boundary_IDs( triangulation );

		// Set boundary IDs and and manifolds
		const Point<dim> direction (0,0,1);
		const Point<dim> centre (0,0,0);
		for (typename Triangulation<dim>::active_cell_iterator
			 cell = triangulation.begin_active();
			 cell != triangulation.end(); ++cell)
		{
			for (unsigned int face=0; face<GeometryInfo<dim>::faces_per_cell; ++face)
			  if (cell->face(face)->at_boundary())
			  {
				//Set boundary IDs
				if (std::abs(cell->face(face)->center()[0] - 0.0) < search_tolerance)
				{
					cell->face(face)->set_boundary_id(enums::id_boundary_xMinus);
				}
				else if (std::abs(cell->face(face)->center()[0] - ratio_width_To_holeRadius) < search_tolerance)
				{
					cell->face(face)->set_boundary_id(enums::id_boundary_xPlus);
				}
				else if (std::abs(cell->face(face)->center()[1] - 0.0) < search_tolerance)
				{
					cell->face(face)->set_boundary_id(enums::id_boundary_yMinus);
					for (unsigned int vertex=0; vertex<GeometryInfo<dim>::vertices_per_face; ++vertex)
					{
					 if (std::abs(cell->vertex(vertex)[enums::y] - 0) < search_tolerance)
						if (std::abs(cell->vertex(vertex)[enums::z] - 0) < search_tolerance)
						  if (std::abs(cell->vertex(vertex)[enums::x] - parameter.holeRadius) < search_tolerance)
						  {
							  // We found the cell that lies at the bottom edge next to the hole (bottom left corner)
							  cell->set_material_id( enums::tracked_QP );
							  break;
						  }
					}
				}
				else if (std::abs(cell->face(face)->center()[1] - ratio_width_To_holeRadius) < search_tolerance)
				{
					cell->face(face)->set_boundary_id(enums::id_boundary_yPlus);
				}
				else if (std::abs(cell->face(face)->center()[2] - 0.0) < search_tolerance)
				{
					cell->face(face)->set_boundary_id(enums::id_boundary_zMinus);
				}
				else if (std::abs(cell->face(face)->center()[2] - parameter.thickness/2.0) < search_tolerance)
				{
					cell->face(face)->set_boundary_id(enums::id_boundary_zPlus);
				}
				else
				{
					for (unsigned int vertex=0; vertex<GeometryInfo<dim>::vertices_per_face; ++vertex)
					 {
					 //Project the cell vertex to the XY plane and test the distance from the cylinder axis
						Point<dim> vertex_proj = cell->vertex(vertex);
						vertex_proj[2] = 0.0;
						if (std::abs(vertex_proj.distance(centre) - parameter.holeRadius) < search_tolerance)
						{
							cell->face(face)->set_boundary_id(parameters_internal.boundary_id_hole);
							break;
						}
					}
				}

				// Set manifold IDs
				for (unsigned int vertex=0; vertex<GeometryInfo<dim>::vertices_per_face; ++vertex)
				{
					//Project the cell vertex to the XY plane and test the distance from the cylinder axis
					Point<dim> vertex_proj = cell->vertex(vertex);
					vertex_proj[2] = 0.0;
					if (std::abs(vertex_proj.distance(centre) - parameter.holeRadius) < search_tolerance)
					{
						//Set manifold ID on face and edges
						cell->face(face)->set_all_manifold_ids(parameters_internal.manifold_id_hole);
						break;
					  }
				  }
			  }
		}

		static SphericalManifold<dim> spherical_manifold (centre);
		triangulation.set_manifold(parameters_internal.manifold_id_hole,spherical_manifold);

		// Pre-refinement (local refinements) of the damaged area (around y=0)
		for (unsigned int refine_counter=0; refine_counter<parameter.nbr_holeEdge_refinements; refine_counter++)
		{
			for (typename Triangulation<dim>::active_cell_iterator
						 cell = triangulation.begin_active();
						 cell != triangulation.end(); ++cell)
			{
				for ( unsigned int face=0; face < GeometryInfo<dim>::faces_per_cell; face++ )
					if ( cell->center()[loading_direction] < ratio_width_To_holeRadius/3. )
					{
						// @note Anisotropic refinements (xy or y) would be ideal here, but don't seem to work in deal.II yet
						 cell->set_refine_flag();
						break;
					}
			}
			triangulation.execute_coarsening_and_refinement();
		}

		if ( parameter.refine_special==1 )
		{
			// Refine the cell(s) marked by tracked_QP \n
			// Strategy: \ņ
			// For 2 global refinements use 3 special refinements
			// For 3 global refinements use 2 special refinement
			// For 4 global refinements use 1 special refinement
			// For everything else, don't refine in this special manner
			for (unsigned int refine_counter=0; int(refine_counter) < std::max(0,int(5-parameter.nbr_global_refinements)); refine_counter++)
			{
				for (typename Triangulation<dim>::active_cell_iterator
							 cell = triangulation.begin_active();
							 cell != triangulation.end(); ++cell)
				{
					for (unsigned int vertex=0; vertex < GeometryInfo<dim>::vertices_per_cell; ++vertex)
					 if (std::abs(cell->vertex(vertex)[enums::y] - 0) < search_tolerance)
						//if (std::abs(cell->vertex(vertex)[enums::z] - 0) < search_tolerance)
						  if (std::abs(cell->vertex(vertex)[enums::x] - parameter.holeRadius) < search_tolerance)
						  {
							  cell->set_refine_flag();
							  break; // Leave this cell
						  }
				}
				triangulation.execute_coarsening_and_refinement();
			}
		}

		// include the following two scopes to see directly how the variation of the input parameters changes the geometry of the grid
		/*
		{
			std::ofstream out ("grid-3d_quarter_plate_merged.eps");
			GridOut grid_out;
			grid_out.write_eps (triangulation, out);
			std::cout << "Grid written to grid-3d_quarter_plate_merged.eps" << std::endl;
		}
		{
			std::ofstream out_ucd("Grid-3d_quarter_plate_merged.inp");
			GridOut grid_out;
			GridOutFlags::Ucd ucd_flags(true,true,true);
			grid_out.set_flags(ucd_flags);
			grid_out.write_ucd(triangulation, out_ucd);
			std::cout<<"Mesh written to Grid-3d_quarter_plate_merged.inp "<<std::endl;
		}
		*/
	}
}
