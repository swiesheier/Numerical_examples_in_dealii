#include <deal.II/grid/tria.h>
#include <deal.II/grid/tria_accessor.h>
#include <deal.II/grid/tria_iterator.h>
#include <deal.II/grid/grid_generator.h>
#include <deal.II/grid/grid_out.h>
#include <deal.II/grid/manifold_lib.h>

#include <iostream>
#include <fstream>
#include <cmath>

// Numerical example helper function (required, can be downloaded from https://github.com/jfriedlein/Numerical_examples_in_dealii)
// also contains enumerators as part of "enums::"
#include "./numEx-helper_fnc.h"

using namespace dealii;

namespace Rod
/*
 * 1/8 of a notched rod in 3D and the axisymmetric half-model in 2D
 * @BUG: The elements for refine_special=3 in the neck do not follow the manifold as the coarser cells do
 * CERTIFIED TO STANDARD numExS07 (200724)
 */
{
	// The loading direction: \n
	// In which coordinate direction the load shall be applied, so x/y/z.
	 const unsigned int loading_direction = enums::y;
//	 const unsigned int loading_direction = enums::x;

	// The loaded faces:
	 const enums::enum_boundary_ids id_boundary_load = enums::id_boundary_yPlus;
	 //const enums::enum_boundary_ids id_boundary_secondaryLoad = enums::id_boundary_xPlus;
//	 const enums::enum_boundary_ids id_boundary_load = enums::id_boundary_xMinus; // internal pressure for pipe

	// Here you can choose between a radial notch (smooth dent) and a sharp triangular notch (viewed in the cross section)
	// USER parameter
	 //const enums::enum_notch_type notch_type = enums::notch_round;
	 const enums::enum_notch_type notch_type = enums::notch_linear;

	// BC
	 // always
	  const enums::enum_BC BC_xMinus  = enums::BC_x0; // symmetry
	  const enums::enum_BC BC_xPlus  = enums::BC_none; // free

	 // NoaR
	  const enums::enum_BC BC_yPlus  = enums::BC_none;  // standard: free

	 // Disk upsetting (sticking)
//	  const enums::enum_BC BC_yPlus  = enums::BC_x0_z0; // special: no contraction of loaded face, sticking contact

	 // special
//	  const enums::enum_BC BC_yPlus  = enums::BC_y0; // special: symmetry condition
//	  const enums::enum_BC BC_xMinus  = enums::BC_none; // free

	 const bool shift_mesh = false;

	// Some internal parameters
	 struct parameterCollection
	 {
		const types::manifold_id manifold_id_surf = 10;

		const double search_tolerance = 1e-8;
	 };

	// Evaluation points: \n
	// We want to points, one for the contraction of the center
	// and one for the contraction of the top face.
	// We don't know the coordinates yet, because the mesh has not yet been created.
	// So we fill the data in make_grid.
	// @todo We need \a dim here instead of 3, but dim is unkown at this place -> redesign
	 std::vector< numEx::EvalPointClass<3> > eval_points_list (2, numEx::EvalPointClass<3>() );


//	 // Wall: Pushing down on the cube
//	  Point<3> wall_point_on_plane = Point<3>(0.0,1.5,0.0);
//	  const Point<3> wall_normal_unit_vector = Point<3>(0,-1.0,0);
//	  std::shared_ptr<WallRigid<3>> rigid_wall = std::shared_ptr<WallRigid<3>>(new WallRigid<3>( {wall_point_on_plane,wall_normal_unit_vector,wall_normal_unit_vector} , {} ));


	// 3D
	template <int dim>
	void make_grid( Triangulation<3> &triangulation, const Parameter::GeneralParameters &parameter )

	{
		// parameterCollection that contains the boundary ids
		 parameterCollection parameters_internal;

		const double search_tolerance = parameters_internal.search_tolerance;

		const double half_length = parameter.width/2.;//53.34/2.;
		const double radius = parameter.holeRadius;//6.4135;
		const double half_notch_length = parameter.notchWidth/2.;//8.98/2.;
		const double notch_radius = parameter.ratio_x * radius; // 0.982

		unsigned int n_additional_refinements = parameter.nbr_holeEdge_refinements;
		unsigned int n_refinements_innermost = 0;
		if ( parameter.refine_special == enums::Mesh_refine_special_innermost )
		{
			n_additional_refinements = 4; // hardcoded to get ...
			n_refinements_innermost = parameter.nbr_holeEdge_refinements; // ... the nbr of local refinements as innermost refinements
		}

		const unsigned int n_global_refinements = parameter.nbr_global_refinements;
		const int n_max_of_elements_in_the_coarse_area = 6;

		// The radius of the notch, e.g. the tool radius that was used to create the notch from the outside
		 const double R = ( half_notch_length*half_notch_length + (radius - notch_radius)*(radius - notch_radius) )
						  / ( 2.*(radius - notch_radius) );

		enum enum_coord_directions
		{
			x = 0, y = 1, z = 2
		};

		Assert(n_additional_refinements>0, ExcMessage("Rod<< Mesh not implemented for only 4 elements in total. Please increase the nbr_holeEdge_refinements to at least 1."));

		// Create in a first step the triangulation representing 1/8 of a cylinder
		 {
			// First we create a cylinder
			 Triangulation<dim> tria_full_cylinder;
			 // @todo Use the new subdivided_cylinder function from deal.ii
			 GridGenerator::cylinder(tria_full_cylinder, radius, half_length);

			// Let's first refine the "cylinder" ones, because the initial mesh is a brick
			 tria_full_cylinder.refine_global( 1 );

			if ( parameter.refine_special == enums::Mesh_refine_uniform )
				tria_full_cylinder.refine_global(n_global_refinements);	// ... Parameter.prm file

			// We rotate the cylinder (oriented along x-axis by default) by 90° (=std::atan(1)*2 rad) around the z-axis
			 GridTools::rotate( std::atan(1)*2, z, tria_full_cylinder);

			// We only model 1/8 of the entire rod, hence we remove everything else
			std::set<typename Triangulation<dim>::active_cell_iterator > cells_to_remove;
			for (typename Triangulation<dim>::active_cell_iterator
				 cell = tria_full_cylinder.begin_active();
				 cell != tria_full_cylinder.end(); ++cell)
			{
				// Remove all cells that are not in the first quadrant.
				// The 1/8 shall reside in the positive x,y,z quadrant
				if (cell->center()[x] < 0.0 || cell->center()[y] < 0.0 || cell->center()[z] < 0.0 )
					cells_to_remove.insert(cell);
			}
			Assert(cells_to_remove.size() > 0, ExcInternalError());
			Assert(cells_to_remove.size() != tria_full_cylinder.n_active_cells(), ExcInternalError());
			GridGenerator::create_triangulation_with_removed_cells(tria_full_cylinder,cells_to_remove,triangulation);
		 }

		// Clear all existing boundary ID's
		 numEx::clear_boundary_IDs( triangulation );


		// Set boundary IDs and and manifolds
		for (typename Triangulation<dim>::active_cell_iterator
			 cell = triangulation.begin_active();
			 cell != triangulation.end(); ++cell)
		{
			for (unsigned int face=0; face<GeometryInfo<dim>::faces_per_cell; ++face)
			 // Cells that describe the boundary can only describe the boundary when they possess a face that lies at the boundary:
			  if (cell->face(face)->at_boundary())
			  {
				// Cell at the x0-plane
				 if (std::abs(cell->face(face)->center()[x] - 0.0) < search_tolerance)
					cell->face(face)->set_boundary_id(enums::id_boundary_xMinus);
				// Cell at the y0-plane
				 else if (std::abs(cell->face(face)->center()[y] - 0.0) < search_tolerance)
						cell->face(face)->set_boundary_id(enums::id_boundary_yMinus);
				// Cell at the z0-plane
				 else if (std::abs(cell->face(face)->center()[z] - 0.0) < search_tolerance)
						cell->face(face)->set_boundary_id(enums::id_boundary_zMinus);
				// Cell at the other end of the rod
				 else if (std::abs(cell->face(face)->center()[y] - half_length) < search_tolerance)
						cell->face(face)->set_boundary_id(enums::id_boundary_yPlus);
				 else
				 {
					  for (unsigned int vertex=0; vertex<GeometryInfo<dim>::vertices_per_face; ++vertex )
					  {
						 // Compute the projected radius in the xz-plane, so the distance between the vertex \a node and the y-axis
						  Point<dim> node = cell->face(face)->vertex(vertex);
						  double distance_2d_xz = std::sqrt( node[x]*node[x] + node[z]*node[z] );

						 if ( std::abs(distance_2d_xz - radius) < search_tolerance )
						 {
							cell->face(face)->set_boundary_id(enums::id_boundary_zPlus);
							break;
						 }
					  }
				 }
			  }
		}

		// Attach a manifold to the curved boundary
		// @todo repair this, inner cells are placed chaotically
		// @note We can only guarantee that the vertices sit on the curve, so we must test with their position instead of the cell centre.
		for (typename Triangulation<dim>::active_cell_iterator
		   cell = triangulation.begin_active();
		   cell != triangulation.end(); ++cell)
		{
		  for (unsigned int face=0; face<GeometryInfo<dim>::faces_per_cell; ++face)
			if (cell->face(face)->at_boundary())
			  for (unsigned int vertex=0; vertex<GeometryInfo<dim>::vertices_per_face; ++vertex)
			  {
				 // Compute the projected radius in the xz-plane, so the distance between the vertex \a node and the y-axis
				  Point<dim> node = cell->face(face)->vertex(vertex);
				  double distance_2d_xz = std::sqrt( node[x]*node[x] + node[z]*node[z] );

				 if ( std::abs(distance_2d_xz - radius) < search_tolerance )
				 {
					// This vertex lies on the outer surface, hence the face and the face belongs to the manifold
					// @note For some reason it is essential to use \æ set_all_manifold_ids() instead of just set_manifold_id
					cell->face(face)->set_all_manifold_ids(parameters_internal.manifold_id_surf);
					break;
				 }
			  }
		}

		// Create a cylindrical manifold to be put on the outer cylindrical surface
		 CylindricalManifold<dim> cylindrical_manifold_3d (y); // y-axis
		 triangulation.set_manifold( parameters_internal.manifold_id_surf, cylindrical_manifold_3d );

		double cell_size_innermost = 9e9;
		if ( parameter.refine_special == enums::Mesh_refine_special_standard || parameter.refine_special == enums::Mesh_refine_special_innermost )
		{
			// Global refinement of the mesh to get a better approximation of the contour:\n
			// Previous: 2 elements for quarter arc; After global refinement: 4 elements
			 triangulation.refine_global( 1 );

			// Add some local refinements:
			// Cells are cut in y-direction, so we simple get some more cells that will be rearranged subsequently
			// @note For some reason I cannot cut_y two cells that lie next to each other. Hence, I only refine the cell at the y0-plane
			 for (unsigned int refine_counter=0; refine_counter < n_additional_refinements; refine_counter++)
			 {
				for (typename Triangulation<dim>::active_cell_iterator
				   cell = triangulation.begin_active();
				   cell != triangulation.end(); ++cell)
				{
					for (unsigned int face=0; face<GeometryInfo<dim>::faces_per_cell; ++face)
						if (cell->face(face)->at_boundary())
							if ( std::abs(cell->face(face)->center()[y])<search_tolerance )
							{
								cell->set_refine_flag(RefinementCase<dim>::cut_y); // refine only in the y-direction
								break;
							}
				}
				triangulation.execute_coarsening_and_refinement();
			 }

		  // Shift the refinement layers in y-direction:
		  // This is a bit tricky and can best be comprehended on paper for specific example values.
			double initial_pos, new_pos;

			const unsigned int nbr_of_y_cells = 4 + n_additional_refinements;
			unsigned int nbr_of_coarse_y_cells = std::min(int(std::ceil(nbr_of_y_cells/2.)),n_max_of_elements_in_the_coarse_area);
			const unsigned int nbr_of_fine_y_cells = nbr_of_y_cells - nbr_of_coarse_y_cells;

			// Shift the coarsest cells such that the coarser outer area is uniformly discretised
			 for ( unsigned int i=1; i<=3; i++ )
			 {
				initial_pos = half_length * (4-i)/4.;
				new_pos = (nbr_of_coarse_y_cells - i)/double(nbr_of_coarse_y_cells) * (half_length - half_notch_length) + half_notch_length;
				numEx::shift_vertex_layer( triangulation, initial_pos, new_pos, y );
			 }

			// We have to grab a few more cells from the local refinements in case we want more than 9 cells in y-direction
			 if ( nbr_of_coarse_y_cells>4 )
				for ( unsigned int i=3; i<=(nbr_of_coarse_y_cells-2); i++ )
				{
					initial_pos = half_length * 1./(std::pow(2,i));
					new_pos = (nbr_of_coarse_y_cells-1 - i)/double(nbr_of_coarse_y_cells) * (half_length - half_notch_length) + half_notch_length;
					numEx::shift_vertex_layer( triangulation, initial_pos, new_pos, y );
				}

			// A small trick to get this general framework to operate even for the two lowest refinements 1 and 2
			 if ( n_additional_refinements <= 2 )
				 nbr_of_coarse_y_cells = 4;

			// Now we are down to the notch length
			 for ( unsigned int i=(nbr_of_coarse_y_cells-1); i<=(n_additional_refinements+2); i++ )
			 {
				initial_pos = half_length * 1./(std::pow(2,i));
				new_pos = (nbr_of_y_cells-1 - i)/double(nbr_of_fine_y_cells)  * half_notch_length;
				numEx::shift_vertex_layer( triangulation, initial_pos, new_pos, y );
			 }

			// We store the size of the innermost cell from the last new_pos
			 cell_size_innermost = new_pos;
		}
		else if ( parameter.refine_special == enums::Rod_refine_special_uniform )
		{
			// Do nothing
		}
		else if ( parameter.refine_special == enums::Mesh_refine_special_Simo )
		{
			// Global refinement of the mesh to get a better approximation of the contour:\n
			// Previous: 2 elements for quarter arc; After global refinement: 4 elements
			 triangulation.refine_global( 1 );

//				for (typename Triangulation<dim>::active_cell_iterator
//				   cell = triangulation.begin_active();
//				   cell != triangulation.end(); ++cell)
//				{
//					for (unsigned int face=0; face<GeometryInfo<dim>::faces_per_cell; ++face)
//						if (cell->face(face)->at_boundary())
//							if ( std::abs(cell->face(face)->center()[y])<search_tolerance )
//							{
//								cell->set_refine_flag();
//								break;
//							}
//				}
//				triangulation.execute_coarsening_and_refinement();

			// Add some local refinements:
			// Cells are cut in y-direction, so we simple get some more cells that will be rearranged subsequently
			// @note For some reason I cannot cut_y two cells that lie next to each other. Hence, I only refine the cell at the y0-plane
			 for (unsigned int refine_counter=0; refine_counter < n_additional_refinements; refine_counter++)
			 {
				for (typename Triangulation<dim>::active_cell_iterator
				   cell = triangulation.begin_active();
				   cell != triangulation.end(); ++cell)
				{
					for (unsigned int face=0; face<GeometryInfo<dim>::faces_per_cell; ++face)
						if (cell->face(face)->at_boundary())
							if ( std::abs(cell->face(face)->center()[y])<search_tolerance )
							{
								cell->set_refine_flag(RefinementCase<dim>::cut_y); // refine only in the y-direction
								break;
							}
				}
				triangulation.execute_coarsening_and_refinement();
			 }
		}
		else if ( parameter.refine_special == enums::Mesh_refine_uniform )
		{
			// nothing
			 for (unsigned int refine_counter=0; refine_counter < n_additional_refinements; refine_counter++)
			 {
				for (typename Triangulation<dim>::active_cell_iterator
				   cell = triangulation.begin_active();
				   cell != triangulation.end(); ++cell)
				{
//					const Point<dim> cell_centre = cell->center();
//					const double radius_xz = std::sqrt( cell_centre[enums::x]*cell_centre[enums::x] + cell_centre[enums::z]*cell_centre[enums::z] );
//					if ( radius_xz  > 0.85*radius )
//						cell->set_refine_flag();
//					for (unsigned int face=0; face<GeometryInfo<dim>::faces_per_cell; ++face)
//						if (cell->face(face)->at_boundary())
//							if ( cell->face(face)->manifold_id() == parameters_internal.manifold_id_surf )
//							{
//								cell->set_refine_flag();
//								break;
//							}
					for (unsigned int vertex=0; vertex < GeometryInfo<dim>::vertices_per_cell; ++vertex)
					{
						const Point<dim> vertex_coord = cell->vertex(vertex);
						const double radius_xz = std::sqrt( vertex_coord[enums::x]*vertex_coord[enums::x] + vertex_coord[enums::z]*vertex_coord[enums::z] );
						if ( std::abs( radius_xz - radius ) < search_tolerance )
						{
							cell->set_refine_flag();
							break;
						}
					}
				}
				triangulation.execute_coarsening_and_refinement();
			 }
		}
//		else if ( parameter.refine_special == enums::Mesh_refine_y )
//		{
//			// Global refinement of the mesh to get a better approximation of the contour:\n
//			// Previous: 2 elements for quarter arc; After global refinement: 4 elements
//			 triangulation.refine_global( 1 );
//
//			// Add some local refinements:
//			// Cells are cut in y-direction, so we simple get some more cells that will be rearranged subsequently
//			// @note For some reason I cannot cut_y two cells that lie next to each other. Hence, I only refine the cell at the y0-plane
//			 for (unsigned int refine_counter=0; refine_counter < n_additional_refinements; refine_counter++)
//			 {
//				for (typename Triangulation<dim>::active_cell_iterator
//				   cell = triangulation.begin_active();
//				   cell != triangulation.end(); ++cell)
//				{
//					for (unsigned int face=0; face<GeometryInfo<dim>::faces_per_cell; ++face)
//						if (cell->face(face)->at_boundary())
//							if ( std::abs(cell->face(face)->center()[y])<search_tolerance )
//							{
//								cell->set_refine_flag(RefinementCase<dim>::cut_y); // refine only in the y-direction
//								break;
//							}
//				}
//				triangulation.execute_coarsening_and_refinement();
//			 }
//
//		  // Shift the refinement layers in y-direction:
//		  // This is a bit tricky and can best be comprehended on paper for specific example values.
//			double initial_pos, new_pos;
//
//			const unsigned int nbr_of_y_cells = 4 + n_additional_refinements;
//			unsigned int nbr_of_coarse_y_cells = std::min(int(std::ceil(nbr_of_y_cells/2.)),n_max_of_elements_in_the_coarse_area);
//			const unsigned int nbr_of_fine_y_cells = nbr_of_y_cells - nbr_of_coarse_y_cells;
//
//			// Shift the coarsest cells such that the coarser outer area is uniformly discretised
//			 for ( unsigned int i=1; i<=3; i++ )
//			 {
//				initial_pos = half_length * (4-i)/4.;
//				new_pos = (nbr_of_coarse_y_cells - i)/double(nbr_of_coarse_y_cells) * (half_length - half_notch_length) + half_notch_length;
//				numEx::shift_vertex_layer( triangulation, initial_pos, new_pos, y );
//			 }
//
//			// We have to grab a few more cells from the local refinements in case we want more than 9 cells in y-direction
//			 if ( nbr_of_coarse_y_cells>4 )
//				for ( unsigned int i=3; i<=(nbr_of_coarse_y_cells-2); i++ )
//				{
//					initial_pos = half_length * 1./(std::pow(2,i));
//					new_pos = (nbr_of_coarse_y_cells-1 - i)/double(nbr_of_coarse_y_cells) * (half_length - half_notch_length) + half_notch_length;
//					numEx::shift_vertex_layer( triangulation, initial_pos, new_pos, y );
//				}
//
//			// A small trick to get this general framework to operate even for the two lowest refinements 1 and 2
//			 if ( n_additional_refinements <= 2 )
//				 nbr_of_coarse_y_cells = 4;
//
//			// Now we are down to the notch length
//			 for ( unsigned int i=(nbr_of_coarse_y_cells-1); i<=(n_additional_refinements+2); i++ )
//			 {
//				initial_pos = half_length * 1./(std::pow(2,i));
//				new_pos = (nbr_of_y_cells-1 - i)/double(nbr_of_fine_y_cells)  * half_notch_length;
//				numEx::shift_vertex_layer( triangulation, initial_pos, new_pos, y );
//			 }
//
//			// We store the size of the innermost cell from the last new_pos
//			 cell_size_innermost = new_pos;
//		}

		// Possibly some additional global isotropic refinements
		// @todo-assure: We shifted these global refinements before the special innermost refinements, so
		// we truely only refine the actual innermost cell.
		if ( parameter.refine_special != enums::Mesh_refine_uniform )
			triangulation.refine_global(n_global_refinements);	// ... Parameter.prm file

		// For the innermost refinement case, we also focus the refinements specifically on the
		// innermost cell, in addition to the above refinement of the notched region
		 if ( parameter.refine_special == enums::Mesh_refine_special_innermost )
		 {
			 for (unsigned int refine_counter=0; refine_counter < n_refinements_innermost; refine_counter++)
			 {
				for (typename Triangulation<dim>::active_cell_iterator
				   cell = triangulation.begin_active();
				   cell != triangulation.end(); ++cell)
				{
					for (unsigned int face=0; face<GeometryInfo<dim>::faces_per_cell; ++face)
						if (cell->face(face)->at_boundary())
							if ( std::abs(cell->face(face)->center()[y]) < search_tolerance )
							{
								if ( refine_counter==0 || refine_counter==2 )
									cell->set_refine_flag(); // isotropic refinement
								else
									cell->set_refine_flag(RefinementCase<dim>::cut_y); // refine only in the y-direction
								//cell->set_refine_flag(RefinementCase<dim>::cut_y); // refine only in the y-direction
								break;
							}
				}
				triangulation.execute_coarsening_and_refinement();
			 }
		 }

		// Generate the notch
		// @note We keep on using the CylindricalManifold from above also for the notched cell faces,
		// which should give us the nice curvature we want.
		 if ( std::abs( parameter.ratio_x - 1. ) > 1e-10 )
			 numEx::notch_body( triangulation, half_notch_length, radius, notch_radius, R, notch_type, true );

		// Output the triangulation as eps or inp
		 //numEx::output_triangulation( triangulation, enums::output_eps, numEx_name );

		// Evaluation points and the related list of them
		 numEx::EvalPointClass<dim> eval_center ( Point<3>(notch_radius,0,0), enums::x );
		 numEx::EvalPointClass<dim> eval_top ( Point<3>(radius,half_length,0), enums::x );

		 eval_points_list = {eval_center,eval_top};
	}
	
	
	// 2d grid
	template <int dim>
	void make_grid( Triangulation<2> &triangulation, const Parameter::GeneralParameters &parameter )
	{
		/*
		 * Input arguments:
		 * * boundary ids and manifold id
		 * * search tolerance
		 * * parameter.width,holeRadius,notchWidth,ratio_x,nbr_holeEdge_refinements,nbr_global_refinements
		 */

		parameterCollection parameters_internal;

		const double search_tolerance = parameters_internal.search_tolerance;

		const double half_length = parameter.width/2.;//53.34/2.;
		const double radius = parameter.holeRadius;//6.4135;
		const double half_notch_length = parameter.notchWidth/2.;//8.98/2.;
		const double notch_radius = parameter.ratio_x * radius; // 0.982

		const int n_max_of_elements_in_the_coarse_area = 6;

		// The radius of the notch, e.g. the tool radius that was used to create the notch from the outside
		 const double R = ( half_notch_length*half_notch_length + (radius - notch_radius)*(radius - notch_radius) )
						  / ( 2.*(radius - notch_radius) );

		 // @todo Somehow merge this and similar enumerator with the global enumerator_list (maybe use flags to detect whether a global enum already exists)
		enum enum_coord_directions
		{
			x = 0, y = 1
		};

		// Create in a first step the triangulation representing 1/8 of a cylinder
		 {
			// First we create a cylinder
			// @todo Can we also create a cylinder in 2D (equals a rectangle, but identical to 3D)
			 Point<dim> p1 (0.,0.);
			 Point<dim> p2 (radius,half_length);

			if ( parameter.refine_special == enums::Mesh_refine_special_standard )
			{
				if ( parameter.nbr_holeEdge_refinements == 0 )
					AssertThrow(parameter.nbr_holeEdge_refinements>0, ExcMessage("Rod<< Mesh not implemented for only 4 elements in total. Please increase the nbr_holeEdge_refinements to at least 1."));

				GridGenerator::hyper_rectangle(triangulation, p1, p2);

				// Let's first refine the "cylinder" ones, because the initial mesh is a brick
				 triangulation.refine_global( 2 );
			}
			else if ( parameter.refine_special == enums::Rod_refine_special_uniform || parameter.refine_special == enums::Mesh_refine_special_Simo )
			{
				 std::vector< unsigned int > repetitions (dim);
				 repetitions[enums::x] = 1;
				 repetitions[enums::y] = 4;
				 GridGenerator::subdivided_hyper_rectangle(triangulation, repetitions, p1, p2);
			}
			else if ( parameter.refine_special == enums::Mesh_refine_none )
			{
				GridGenerator::subdivided_hyper_rectangle(triangulation, {4,1}, p1, p2);
			}
			else if ( parameter.refine_special == enums::Mesh_Rod_Upsetting_tapered )
			{
				// The number of elements are chosen to match the tapering exactly
				 GridGenerator::subdivided_hyper_rectangle(triangulation, {10,15}, p1, p2);
			}
			else if ( parameter.refine_special == enums::Mesh_Rod_ax_ratio_EL )
			{
				const unsigned int n_elements_x = parameter.nbr_elementsInZ*parameter.grid_y_repetitions;
				const unsigned int n_elements_y = parameter.grid_y_repetitions;
				 GridGenerator::subdivided_hyper_rectangle(triangulation, {n_elements_x,n_elements_y}, p1, p2);
			}
		 }

		// Clear all existing boundary ID's
		 numEx::clear_boundary_IDs( triangulation );

		// Set boundary IDs and and manifolds
		 for (typename Triangulation<dim>::active_cell_iterator
			 cell = triangulation.begin_active();
			 cell != triangulation.end(); ++cell)
		 {
			for (unsigned int face=0; face<GeometryInfo<dim>::faces_per_cell; ++face)
			 // Cells that describe the boundary can only describe the boundary when they possess a face that lies at the boundary:
			  if (cell->face(face)->at_boundary())
			  {
				// Faces at the x0-plane
				 if (std::abs(cell->face(face)->center()[x] - 0.0) < search_tolerance)
					cell->face(face)->set_boundary_id(enums::id_boundary_xMinus);
				// Faces at right end
				 else if (std::abs(cell->face(face)->center()[x] - radius) < search_tolerance)
					cell->face(face)->set_boundary_id(enums::id_boundary_xPlus);
				// Faces at the y0-plane
				 else if (std::abs(cell->face(face)->center()[y] - 0.0) < search_tolerance)
					cell->face(face)->set_boundary_id(enums::id_boundary_yMinus);
				// Faces at the other y-end of the rod
				 else if (std::abs(cell->face(face)->center()[y] - half_length) < search_tolerance)
					cell->face(face)->set_boundary_id(enums::id_boundary_yPlus);
			  }
		 }

		// Shift the mesh after we have identified the boundary ids,
		// so the determination is still independent of the actual shift
		 if ( shift_mesh )
		 {
			// Shift mesh to create e.g. a pipe
			 Tensor<1,dim> shift_vector;
			 shift_vector[enums::x]=4;//4;
			 GridTools::shift(shift_vector,triangulation);
		 }

		if ( parameter.refine_special == enums::Mesh_refine_special_standard )
		{
			// Add some local refinements:
			// Cells are cut in y-direction, so we simple get some more cells that will be rearranged subsequently
			// @note For some reason I cannot cut_y two cells that lie next to each other. Hence, I only refine the cell at the y0-plane
			 for (unsigned int refine_counter=0; refine_counter<parameter.nbr_holeEdge_refinements; refine_counter++)
			 {
				for (typename Triangulation<dim>::active_cell_iterator
				   cell = triangulation.begin_active();
				   cell != triangulation.end(); ++cell)
				{
					for (unsigned int face=0; face<GeometryInfo<dim>::faces_per_cell; ++face)
						if (cell->face(face)->at_boundary())
							if ( std::abs(cell->face(face)->center()[y])<search_tolerance )
							{
								cell->set_refine_flag(RefinementCase<dim>::cut_y); // refine only in the y-direction
								break;
							}
				}
				triangulation.execute_coarsening_and_refinement();
			 }

		  // ToDo-optimize: Isn't this very similar to the 3D case? Maybe merge 2D and 3D, also the surrounding code seems familiar
		 // @todo Check use of anisotropic refinements for neighbouring elements instead of this splitting and shifting
			// @todo Also consider the use of dII subdivided_hyper_rectangle with step_sizes for "graded meshes"
		  // Shift the refinement layers in y-direction:
		  // This is a bit tricky and can best be comprehended on paper for specific example values.
			double initial_pos, new_pos;

			const unsigned int nbr_of_y_cells = 4 + parameter.nbr_holeEdge_refinements;
			unsigned int nbr_of_coarse_y_cells = std::min(int(std::ceil(nbr_of_y_cells/2.)),n_max_of_elements_in_the_coarse_area);
			const unsigned int nbr_of_fine_y_cells = nbr_of_y_cells - nbr_of_coarse_y_cells;

			// Shift the coarsest cells such that the coarser outer area is uniformly discretised
			 for ( unsigned int i=1; i<=3; i++ )
			 {
				initial_pos = half_length * (4-i)/4.;
				new_pos = (nbr_of_coarse_y_cells - i)/double(nbr_of_coarse_y_cells) * (half_length - half_notch_length) + half_notch_length;
				numEx::shift_vertex_layer( triangulation, initial_pos, new_pos, y );
			 }

			// We have to grab a few more cells from the local refinements in case we want more than 9 cells in y-direction
			 if ( nbr_of_coarse_y_cells>4 )
				for ( unsigned int i=3; i<=(nbr_of_coarse_y_cells-2); i++ )
				{
					initial_pos = half_length * 1./(std::pow(2,i));
					new_pos = (nbr_of_coarse_y_cells-1 - i)/double(nbr_of_coarse_y_cells) * (half_length - half_notch_length) + half_notch_length;
					numEx::shift_vertex_layer( triangulation, initial_pos, new_pos, y );
				}

			// A small trick to get this general framework to operate even for the two lowest refinements 1 and 2
			 if ( parameter.nbr_holeEdge_refinements<=2 )
				 nbr_of_coarse_y_cells = 4;

			// Now we are down to the notch length
			 for ( unsigned int i=(nbr_of_coarse_y_cells-1); i<=(parameter.nbr_holeEdge_refinements+2); i++ )
			 {
				initial_pos = half_length * 1./(std::pow(2,i));
				new_pos = (nbr_of_y_cells-1 - i)/double(nbr_of_fine_y_cells)  * half_notch_length;
				numEx::shift_vertex_layer( triangulation, initial_pos, new_pos, y );
			 }
		}
		else if ( parameter.refine_special == enums::Rod_refine_special_uniform )
		{
//			 triangulation.refine_global( 2 );
		}
		else if ( parameter.refine_special == enums::Mesh_refine_special_Simo )
		{
			 for (unsigned int refine_counter=0; refine_counter<parameter.nbr_holeEdge_refinements; refine_counter++)
			 {
				for (typename Triangulation<dim>::active_cell_iterator
				   cell = triangulation.begin_active();
				   cell != triangulation.end(); ++cell)
				{
					for (unsigned int face=0; face<GeometryInfo<dim>::faces_per_cell; ++face)
						if ( cell->face(face)->at_boundary() && cell->face(face)->boundary_id()==enums::id_boundary_yMinus )
							{
								cell->set_refine_flag(RefinementCase<dim>::cut_y); // refine only in the y-direction
								break;
							}
				}
				triangulation.execute_coarsening_and_refinement();
			 }
		}
		else if ( parameter.refine_special == enums::Mesh_Rod_ax_ratio_EL )
		{
			 for (unsigned int refine_counter=0; refine_counter<parameter.nbr_holeEdge_refinements; refine_counter++)
			 {
				for (typename Triangulation<dim>::active_cell_iterator
				   cell = triangulation.begin_active();
				   cell != triangulation.end(); ++cell)
				{
					if ( cell->center()[enums::x] > radius*0.9 )
					{
						cell->set_refine_flag();
					}
				}
				triangulation.execute_coarsening_and_refinement();
			 }
		}

		// Generate the notch
		if ( true /*standard taper*/)
		{
			 numEx::notch_body( triangulation, half_notch_length, radius, notch_radius, R, notch_type, true );

			// Evaluation points and the related list of them
			 numEx::EvalPointClass<3> eval_center ( Point<3>(notch_radius,0,0), enums::x );
			 numEx::EvalPointClass<3> eval_top ( Point<3>(radius,half_length,0), enums::x );
			 eval_points_list = {eval_center,eval_top};
		}
		else // @todo What is this?
		{
//			  const double notch_reduction = parameter.ratio_x;
//			  Point<3> notch_reference_point1 ( radius, half_length, 0);
//			  Point<3> face_normal1(1.,0,0);
//			  double notch_depth = (1.-notch_reduction)*radius;
//
//			  numEx::NotchClass<2> notch1 ( enums::notch_linear, parameter.notchWidth, notch_depth, notch_reference_point1, enums::id_boundary_xPlus,
//											face_normal1, enums::y);
//
//			  numEx::notch_body( triangulation, notch1 );

			 const double offset = half_length;
			 numEx::notch_body( triangulation, half_notch_length, radius, notch_radius, R, notch_type, true, offset );

			// Evaluation points and the related list of them
			 numEx::EvalPointClass<3> eval_center ( Point<3>(radius,0,0), enums::x );
			 numEx::EvalPointClass<3> eval_top ( Point<3>(0,half_length,0), enums::x );
			 eval_points_list = {eval_center,eval_top};
		}

		// Possibly some additional global isotropic refinements
		 triangulation.refine_global(parameter.nbr_global_refinements);	// ... Parameter.prm file

		// Output the triangulation as eps or inp
		 //numEx::output_triangulation( triangulation, enums::output_eps, numEx_name );
	}


	template<int dim>
	void make_constraints ( AffineConstraints<double> &constraints, const FESystem<dim> &fe, unsigned int &n_components, DoFHandler<dim> &dof_handler_ref,
							const bool &apply_dirichlet_bc, double &current_load_increment,
							const Parameter::GeneralParameters &parameter)
	{
		// BC on x0 plane
		 if ( BC_xMinus==enums::BC_x0 )
			numEx::BC_apply( enums::id_boundary_xMinus, enums::x, 0, apply_dirichlet_bc, dof_handler_ref, fe, constraints );

		// BC on xPlus plane
		 if ( BC_xPlus==enums::BC_x0 )
			numEx::BC_apply( enums::id_boundary_xPlus, enums::x, 0, apply_dirichlet_bc, dof_handler_ref, fe, constraints );

		// BC on y0 plane
		 numEx::BC_apply( enums::id_boundary_yMinus, enums::y, 0, apply_dirichlet_bc, dof_handler_ref, fe, constraints );

		// BC on z0 plane
		 if ( dim==3 )
			numEx::BC_apply( enums::id_boundary_zMinus, enums::z, 0, apply_dirichlet_bc, dof_handler_ref, fe, constraints );

		// BC for the yPlus
		 if ( BC_yPlus==enums::BC_x0_z0 ) // no contraction
		 {
			numEx::BC_apply( enums::id_boundary_yPlus, enums::x, 0, apply_dirichlet_bc, dof_handler_ref, fe, constraints );
			if ( dim==3 )
				numEx::BC_apply( enums::id_boundary_yPlus, enums::z, 0, apply_dirichlet_bc, dof_handler_ref, fe, constraints );
		 }
		 else if ( BC_yPlus==enums::BC_y0 )
			numEx::BC_apply( enums::id_boundary_yPlus, enums::y, 0, apply_dirichlet_bc, dof_handler_ref, fe, constraints );

		// BC for the load ...
		 if ( parameter.driver == enums::Dirichlet )  // ... as Dirichlet only for Dirichlet as driver
			numEx::BC_apply( id_boundary_load, loading_direction, current_load_increment, apply_dirichlet_bc, dof_handler_ref, fe, constraints );
//		 else if ( parameter.driver == enums::Contact ) // ... as contact
//		 {
//			if (apply_dirichlet_bc == true )
//				rigid_wall->move( current_load_increment );
//	 	 }
	}



	// 3d grid
	/*
	 * @param triangulation
	 * @param length_of_the_entireRod The length of the entire rod
	 * @param radius_of_the_entireRod The outer radius of the rod
	 * @param length_of_the_entireNotchedArea The length of the notch in y-direction. We only model 1/8 of the entire bar, hence only 1/2 of the notch length
	 *
	 * @todo Add the remaining parameters with description
	 */
	template <int dim>
	void make_grid (
						Triangulation<3> &triangulation,
						const double &length_of_the_entireRod,
						const double &radius_of_the_entireRod,
						const double &length_of_the_entireNotchedArea,
						const double &radius_reductionFactor_in_notchedArea,
						const unsigned int n_additonal_refinements_in_y=1,
						const unsigned int n_global_refinements=0,
						const int n_max_of_elements_in_the_coarse_area = 6
				   )

	{
		// parameterCollection that contains the boundary ids
		 parameterCollection parameters_internal;

		const double search_tolerance = parameters_internal.search_tolerance;

		const double half_length = length_of_the_entireRod/2.;
		const double radius = radius_of_the_entireRod;
		const double half_notch_length = length_of_the_entireNotchedArea/2.;
		const double notch_radius = radius_reductionFactor_in_notchedArea * radius;
		const unsigned int n_additional_refinements = n_additonal_refinements_in_y;

		// The radius of the notch, e.g. the tool radius that was used to create the notch from the outside
		 const double R = ( half_notch_length*half_notch_length + (radius - notch_radius)*(radius - notch_radius) )
						  / ( 2.*(radius - notch_radius) );

		enum enum_coord_directions
		{
			x = 0, y = 1, z = 2
		};

		Assert(n_additional_refinements>0, ExcMessage("Rod<< Mesh not implemented for only 4 elements in total. Please increase the nbr_holeEdge_refinements to at least 1."));

		// Create in a first step the triangulation representing 1/8 of a cylinder
		 {
			// First we create a cylinder
			 Triangulation<dim> tria_full_cylinder;
			 // @todo Use the new subdivided_cylinder function from deal.ii
			 GridGenerator::cylinder(tria_full_cylinder, radius, half_length);

			// Let's first refine the "cylinder" ones, because the initial mesh is a brick
			 tria_full_cylinder.refine_global( 1 );

			// We rotate the cylinder (oriented along x-axis by default) by 90° (=std::atan(1)*2 rad) around the z-axis
			 GridTools::rotate( std::atan(1.)*2., z, tria_full_cylinder);

			// We only model 1/8 of the entire rod, hence we remove everything else
			 std::set<typename Triangulation<dim>::active_cell_iterator > cells_to_remove;
			 for (typename Triangulation<dim>::active_cell_iterator
				 cell = tria_full_cylinder.begin_active();
				 cell != tria_full_cylinder.end(); ++cell)
			 {
				// Remove all cells that are not in the first quadrant.
				// The 1/8 shall reside in the positive x,y,z quadrant
				if (cell->center()[x] < 0.0 || cell->center()[y] < 0.0 || cell->center()[z] < 0.0 )
					cells_to_remove.insert(cell);
			 }
			 Assert(cells_to_remove.size() > 0, ExcInternalError());
			 Assert(cells_to_remove.size() != tria_full_cylinder.n_active_cells(), ExcInternalError());
			 GridGenerator::create_triangulation_with_removed_cells(tria_full_cylinder,cells_to_remove,triangulation);
		 }

		// Clear all existing boundary ID's
		 numEx::clear_boundary_IDs( triangulation );

		// Set boundary IDs and and manifolds
		 for (typename Triangulation<dim>::active_cell_iterator
			 cell = triangulation.begin_active();
			 cell != triangulation.end(); ++cell)
		 {
			for (unsigned int face=0; face<GeometryInfo<dim>::faces_per_cell; ++face)
			 // Cells that describe the boundary can only describe the boundary when they possess a face that lies at the boundary:
			  if (cell->face(face)->at_boundary())
			  {
				// Cell at the x0-plane
				 if (std::abs(cell->face(face)->center()[x] - 0.0) < search_tolerance)
					cell->face(face)->set_boundary_id(enums::id_boundary_xMinus);
				// Cell at the y0-plane
				 else if (std::abs(cell->face(face)->center()[y] - 0.0) < search_tolerance)
						cell->face(face)->set_boundary_id(enums::id_boundary_yMinus);
				// Cell at the z0-plane
				 else if (std::abs(cell->face(face)->center()[z] - 0.0) < search_tolerance)
						cell->face(face)->set_boundary_id(enums::id_boundary_zMinus);
				// Cell at the other end of the rod
				 else if (std::abs(cell->face(face)->center()[y] - half_length) < search_tolerance)
						cell->face(face)->set_boundary_id(enums::id_boundary_yPlus);
			  }
		 }

		// Attach a manifold to the curved boundary
		// @note We can only guarantee that the vertices sit on the curve, so we must test with their position instead of the cell centre.
		for (typename Triangulation<dim>::active_cell_iterator
		   cell = triangulation.begin_active();
		   cell != triangulation.end(); ++cell)
		{
		  for (unsigned int face=0; face<GeometryInfo<dim>::faces_per_cell; ++face)
			if (cell->face(face)->at_boundary())
			  for (unsigned int vertex=0; vertex<GeometryInfo<dim>::vertices_per_face; ++vertex)
			  {
				 // Compute the projected radius in the xz-plane, so the distance between the vertex and the y-axis
				  double distance_2d_xz = std::sqrt( cell->vertex(vertex)[x]*cell->vertex(vertex)[x] + cell->vertex(vertex)[z]*cell->vertex(vertex)[z] );
				 if ( std::abs(distance_2d_xz - radius) < search_tolerance )
				 {
					// This vertex lies on the outer surface, hence the face belongs to the manifold
					// @note For some reason it is essential to use \a set_all_manifold_ids() instead of just set_manifold_id
					cell->face(face)->set_all_manifold_ids(parameters_internal.manifold_id_surf);
					break;
				 }
			  }
		}

		// Create a cylindrical manifold to be put on the outer cylindrical surface
		 CylindricalManifold<dim> cylindrical_manifold_3d (y); // y-axis
		 triangulation.set_manifold( parameters_internal.manifold_id_surf, cylindrical_manifold_3d );

		// Global refinement of the mesh to get a better approximation of the contour:\n
		// Previous: 2 elements for quarter arc; After global refinement: 4 elements
		 triangulation.refine_global( 1 );

		// Add some local refinements:
		// Cells are cut in y-direction, so we simple get some more cells that will be rearranged subsequently
		// @note For some reason I cannot cut_y two cells that lie next to each other. Hence, I only refine the cell at the y0-plane
		 for (unsigned int refine_counter=0; refine_counter < n_additional_refinements; refine_counter++)
		 {
			for (typename Triangulation<dim>::active_cell_iterator
			   cell = triangulation.begin_active();
			   cell != triangulation.end(); ++cell)
			{
				for (unsigned int face=0; face<GeometryInfo<dim>::faces_per_cell; ++face)
					if (cell->face(face)->at_boundary())
						if ( std::abs(cell->face(face)->center()[y])<search_tolerance )
						{
							cell->set_refine_flag(RefinementCase<dim>::cut_y); // refine only in the y-direction
							break;
						}
			}
			triangulation.execute_coarsening_and_refinement();
		 }

	  // Shift the refinement layers in y-direction:
	  // This is a bit tricky and can best be comprehended on paper for specific example values.
		double initial_pos, new_pos;

		const unsigned int nbr_of_y_cells = 4 + n_additional_refinements;
		unsigned int nbr_of_coarse_y_cells = std::min(int(std::ceil(nbr_of_y_cells/2.)),n_max_of_elements_in_the_coarse_area);
		const unsigned int nbr_of_fine_y_cells = nbr_of_y_cells - nbr_of_coarse_y_cells;

		// Shift the coarsest cells such that the coarser outer area is uniformly discretised
		 for ( unsigned int i=1; i<=3; i++ )
		 {
			initial_pos = half_length * (4-i)/4.;
			new_pos = (nbr_of_coarse_y_cells - i)/double(nbr_of_coarse_y_cells) * (half_length - half_notch_length) + half_notch_length;
			numEx::shift_vertex_layer( triangulation, initial_pos, new_pos, y );
		 }

		// We have to grab a few more cells from the local refinements in case we want more than 9 cells in y-direction
		 if ( nbr_of_coarse_y_cells>4 )
			for ( unsigned int i=3; i<=(nbr_of_coarse_y_cells-2); i++ )
			{
				initial_pos = half_length * 1./(std::pow(2,i));
				new_pos = (nbr_of_coarse_y_cells-1 - i)/double(nbr_of_coarse_y_cells) * (half_length - half_notch_length) + half_notch_length;
				numEx::shift_vertex_layer( triangulation, initial_pos, new_pos, y );
			}

		// A small trick to get this general framework to operate even for the two lowest refinements 1 and 2
		 if ( n_additional_refinements <= 2 )
			 nbr_of_coarse_y_cells = 4;

		// Now we are down to the notch length
		 for ( unsigned int i=(nbr_of_coarse_y_cells-1); i<=(n_additional_refinements+2); i++ )
		 {
			initial_pos = half_length * 1./(std::pow(2,i));
			new_pos = (nbr_of_y_cells-1 - i)/double(nbr_of_fine_y_cells)  * half_notch_length;
			numEx::shift_vertex_layer( triangulation, initial_pos, new_pos, y );
		 }

		// Generate the notch
		 numEx::notch_body( triangulation, half_notch_length, radius, notch_radius, R, notch_type, true );

		// Possibly some additional global isotropic refinements
		 triangulation.refine_global(n_global_refinements);	// ... Parameter.prm file

	//		// include the following two scopes to see directly how the variation of the input parameters changes the geometry of the grid
	//		{
	//			std::ofstream out ("grid-3d_quarter_plate_merged.eps");
	//			GridOut grid_out;
	//			grid_out.write_eps (triangulation, out);
	//			std::cout << "Grid written to grid-3d_quarter_plate_merged.eps" << std::endl;
	//		}
	//		{
	//			std::ofstream out_ucd("Grid-3d_quarter_plate_merged.inp");
	//			GridOut grid_out;
	//			GridOutFlags::Ucd ucd_flags(true,true,true);
	//			grid_out.set_flags(ucd_flags);
	//			grid_out.write_ucd(triangulation, out_ucd);
	//			std::cout<<"Mesh written to Grid-3d_quarter_plate_merged.inp "<<std::endl;
	//		}
	}

	template <int dim>
	void assemble_contact
	(
			const typename DoFHandler<dim>::active_cell_iterator &cell,
			const double &penalty_stiffness,
			const FESystem<dim> &fe,
			FEFaceValues<dim> &fe_face_values_ref,
			const FEValuesExtractors::Vector u_fe,
			const unsigned int n_q_points_f,
			const Vector<double> &current_solution,
			std::vector< std::shared_ptr< PointHistory<dim> > > lqph,
			const std::vector<types::global_dof_index> local_dof_indices,
			FullMatrix<double> &cell_matrix,
			Vector<double> &cell_rhs
	)
	{
//		// Assemble the contact pair
//		assemble_contact(
//							cell,
//							rigid_wall,
//							{enums::id_boundary_yPlus,enums::id_boundary_zPlus},
////							enums::id_boundary_yPlus,
//							penalty_stiffness,
//							fe,
//							fe_face_values_ref,
//							u_fe,
//							n_q_points_f,
//							current_solution,
//							lqph,
//							local_dof_indices,
//							cell_matrix,
//							cell_rhs
//						 );
	}
}
