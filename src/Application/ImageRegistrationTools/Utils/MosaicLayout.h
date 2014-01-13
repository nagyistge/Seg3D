/*
 For more information, please see: http://software.sci.utah.edu
 
 The MIT License
 
 Copyright (c) 2013 Scientific Computing and Imaging Institute,
 University of Utah.
 
 
 Permission is hereby granted, free of charge, to any person obtaining a
 copy of this software and associated documentation files (the "Software"),
 to deal in the Software without restriction, including without limitation
 the rights to use, copy, modify, merge, publish, distribute, sublicense,
 and/or sell copies of the Software, and to permit persons to whom the
 Software is furnished to do so, subject to the following conditions:
 
 The above copyright notice and this permission notice shall be included
 in all copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 DEALINGS IN THE SOFTWARE.
*/

// File         : mosaic_layout_common.hxx
// Author       : Pavel A. Koshevoy
// Created      : Thu Mar 22 13:09:33 MDT 2007
// Copyright    : (C) 2004-2008 University of Utah
// Description  : Helper functions for automatic mosaic layout.

#ifndef APPLICATION_IMAGEREGISTRATIONTOOLS_UTILS_MOSAICLAYOUT_H
#define APPLICATION_IMAGEREGISTRATIONTOOLS_UTILS_MOSAICLAYOUT_H

// the includes:
//#include <itk/common.hxx>
#include <itk/fft_common.hxx>
#include <itk/grid_common.hxx>
#include <itk/itkLegendrePolynomialTransform.h>
#include <itk/itkRegularStepGradientDescentOptimizer2.h>
#include <itk/itkImageMosaicVarianceMetric.h>
#include <utils/the_utils.hxx>
//#include <utils/the_log.hxx>

// ITK includes:
#include <itkCommand.h>
#include <itkImageMaskSpatialObject.h>
#include <itkImageRegistrationMethod.h>
#include <itkMeanSquaresImageToImageMetric.h>


//----------------------------------------------------------------
// affine_transform_t
// 
typedef itk::LegendrePolynomialTransform<double, 1> affine_transform_t;


//----------------------------------------------------------------
// reset
// 
extern void
reset(const unsigned int num_images,
      const unsigned int max_cascade_len,
      array3d(translate_transform_t::Pointer) & path,
      array3d(double) & cost);

//----------------------------------------------------------------
// setup_transform
// 
template <typename TImage>
affine_transform_t::Pointer
setup_transform(const TImage * image,
                const base_transform_t * t0,
                const base_transform_t * t1 = 0,
                const unsigned int samples = 16)
{
// TODO: rewrite!
// shortcuts:
#define inv_transform                                                        \
inverse_transform<TImage, std::vector<base_transform_t::Pointer> >
  
#define fwd_transform                                                        \
forward_transform<TImage, std::vector<const base_transform_t *> >
  
  typedef typename TImage::IndexType index_t;
  index_t i00;
  i00[0] = 0;
  i00[1] = 0;
  
  typename TImage::PointType origin;
  image->TransformIndexToPhysicalPoint(i00, origin);
  
  index_t i11;
  i11[0] = 1;
  i11[1] = 1;
  
  typename TImage::PointType spacing;
  image->TransformIndexToPhysicalPoint(i11, spacing);
  spacing[0] -= origin[0];
  spacing[1] -= origin[1];
  
  typename TImage::SizeType sz = image->GetLargestPossibleRegion().GetSize();
  
  typename TImage::PointType tile_min = origin;
  typename TImage::PointType tile_max;
  tile_max[0] = tile_min[0] + spacing[0] * double(sz[0]);
  tile_max[1] = tile_min[1] + spacing[1] * double(sz[1]);
  
  double w = sz[0] * spacing[0];
  double h = sz[1] * spacing[1];
  double Umax = w / 2.0;
  double Vmax = h / 2.0;
  
  affine_transform_t::Pointer affine = affine_transform_t::New();
  affine->setup(tile_min[0],
                tile_max[0],
                tile_min[1],
                tile_max[1],
                Umax,
                Vmax);
  
  std::vector<const base_transform_t *> fwd_cascade(2);
  std::vector<base_transform_t::Pointer> inv_cascade(2);
  fwd_cascade[0] = t0;
  fwd_cascade[1] = t1;
  
  inv_cascade[0] = t0->GetInverse();
  inv_cascade[1] = (t1 == 0) ? 0 : t1->GetInverse();
  
  const unsigned int cascade_len = (t1 == 0) ? 1 : 2;
  
  // calculate the shift:
  pnt2d_t center = pnt2d(tile_min[0] + (tile_max[0] - tile_min[0]) / 2.0,
                         tile_min[1] + (tile_max[1] - tile_min[1]) / 2.0);
  pnt2d_t transformed_center = inv_transform(inv_cascade, cascade_len, center);
  vec2d_t shift = center - transformed_center;
  affine->setup_translation(shift[0], shift[1]);
  
  // initialize the local bounding box:
  typename TImage::PointType mosaic_min;
  typename TImage::PointType mosaic_max;
  mosaic_min[0] = std::numeric_limits<double>::max();
  mosaic_min[1] = mosaic_min[0];
  mosaic_max[0] = -mosaic_min[0];
  mosaic_max[1] = -mosaic_min[0];
  
  // find out where this image is mapped to in the mosaic
  // by the given transform:
  {
    // a handy shortcut:
    typename TImage::PointType tile_point;
// TODO: rewrite!
#define UPDATE_BBOX( u, v )                                                \
tile_point[0] = tile_min[0] + u * (tile_max[0] - tile_min[0]);        \
tile_point[1] = tile_min[1] + v * (tile_max[1] - tile_min[1]);        \
update_bbox(mosaic_min, mosaic_max,                                        \
inverse_transform<TImage,                                \
std::vector<base_transform_t::Pointer> >                \
(inv_cascade, cascade_len, tile_point))
    
    // image corners:
    UPDATE_BBOX(0.0, 0.0);
    UPDATE_BBOX(1.0, 0.0);
    UPDATE_BBOX(0.0, 1.0);
    UPDATE_BBOX(1.0, 1.0);
    
    // points along the image edges:
    for (unsigned int x = 0; x < samples; x++)
    {
      const double t = double(x + 1) / double(samples + 1);
      
      UPDATE_BBOX(t, 0.0);
      UPDATE_BBOX(t, 1.0);
      UPDATE_BBOX(0.0, t);
      UPDATE_BBOX(1.0, t);
    }
#undef UPDATE_BBOX
  }
  
  const double W = mosaic_max[0] - mosaic_min[0];
  const double H = mosaic_max[1] - mosaic_min[1];
  
  std::vector<pnt2d_t> uv(samples * samples);
  std::vector<pnt2d_t> xy(samples * samples);
  
  // explore the transform with jittered sampling:
  for (unsigned int i = 0; i < samples; i++)
  {
    for (unsigned int j = 0; j < samples; j++)
    {
      const unsigned int k = i * samples + j;
      
      uv[k][0] =
      mosaic_min[0] + W * (double(i) + drand()) / double(samples);
      uv[k][1] =
      mosaic_min[1] + H * (double(j) + drand()) / double(samples);
      
      xy[k] = fwd_transform(fwd_cascade, cascade_len, uv[k]);
    }
  }
  
#undef inv_transform
#undef fwd_transform
  
  affine->solve_for_parameters(0, affine_transform_t::Degree + 1, uv, xy);
  return affine;
}

//----------------------------------------------------------------
// assemble_cascades
// 
extern void
assemble_cascades(const unsigned int num_images,
                  const unsigned int max_cascade_len,
                  array3d(translate_transform_t::Pointer) & path,
                  array3d(double) & cost,
                  const bool & cumulative_cost = false);

//----------------------------------------------------------------
// establish_mappings
// 
extern void
establish_mappings(/*the_log_t & log,*/
                   
                   const unsigned int num_images,
                   const unsigned int max_cascade_len,
                   const array3d(translate_transform_t::Pointer) & path,
                   const array3d(double) & cost,
                   array2d(translate_transform_t::Pointer) & mapping,
                   array2d(double) & mapping_cost);



//----------------------------------------------------------------
// refine_one_pair
//
template <typename TImage, typename TMask>
double
refine_one_pair(/*the_log_t & log,*/
                
                const TImage * i0,
                const TImage * i1,
                const TMask * m0,
                const TMask * m1,
                translate_transform_t::Pointer & t01,
                
                const unsigned int iterations,
                const double & min_step,
                const double & max_step,
                
                const the_text_t & fn_prefix = the_text_t(""))
{
  // setup the registration object:
  typedef itk::ImageRegistrationMethod<TImage, TImage> registration_t;
  typename registration_t::Pointer registration = registration_t::New();
  
  // setup the image interpolator:
  typedef itk::LinearInterpolateImageFunction<TImage, double> interpolator_t;
  registration->SetInterpolator(interpolator_t::New());
  
  if (fn_prefix.size() != 0)
  {
    save_rgb<TImage>(fn_prefix + "a.tif", i0, i1, t01, m0, m1);
  }
  
  // setup the optimizer:
  optimizer_t::Pointer optimizer = optimizer_t::New();
  registration->SetOptimizer(optimizer);
  optimizer_observer_t<optimizer_t>::Pointer observer =
  optimizer_observer_t<optimizer_t>::New();
//  observer->log_ = &log;
//  optimizer->SetLog(&log);
  optimizer->AddObserver(itk::IterationEvent(), observer);
  optimizer->SetMinimize(true);
  optimizer->SetNumberOfIterations(iterations);
  optimizer->SetMinimumStepLength(min_step);
  optimizer->SetMaximumStepLength(max_step);
  optimizer->SetGradientMagnitudeTolerance(1e-6);
  optimizer->SetRelaxationFactor(5e-1);
  optimizer->SetPickUpPaceSteps(5);
  optimizer->SetBackTracking(true);
  
  // FIXME: this is probably unnecessary:
  typedef optimizer_t::ScalesType optimizer_scales_t;
  optimizer_scales_t translation_scales(t01->GetNumberOfParameters());
  translation_scales.Fill(1.0);
  
  try { optimizer->SetScales(translation_scales); }
  catch (itk::ExceptionObject & exception) {}
  
  // setup the image-to-image metric:
  typedef itk::MeanSquaresImageToImageMetric<TImage, TImage> metric_t;
  
  typename metric_t::Pointer metric = metric_t::New();
  registration->SetMetric(metric);
  
  registration->SetTransform(t01);
  registration->SetInitialTransformParameters(t01->GetParameters());
  
  // setup the masks:
  typedef itk::ImageMaskSpatialObject<2> mask_so_t;
  typename TMask::ConstPointer fi_mask = m0;
  if (m0 != 0)
  {
    mask_so_t::Pointer fi_mask_so = mask_so_t::New();
    fi_mask_so->SetImage(fi_mask);
    metric->SetFixedImageMask(fi_mask_so);
  }
  
  typename TMask::ConstPointer mi_mask = m1;
  if (m1 != 0)
  {
    mask_so_t::Pointer mi_mask_so = mask_so_t::New();
    mi_mask_so->SetImage(mi_mask);
    metric->SetMovingImageMask(mi_mask_so);
  }
  
  // setup the fixed and moving image:
  typename TImage::ConstPointer fi = i0;
  typename TImage::ConstPointer mi = i1;
  
  registration->SetFixedImageRegion(fi->GetLargestPossibleRegion());
  registration->SetFixedImage(fi);
  registration->SetMovingImage(mi);
  
  // evaluate the metric before the registration:
  double metric_before =
  eval_metric<metric_t, interpolator_t>(t01, fi, mi, fi_mask, mi_mask);
  assert(metric_before != std::numeric_limits<double>::max());
  
  translate_transform_t::ParametersType params_before =
  t01->GetParameters();
  
  // perform the registration:
  try
  {
//    log << endl;
    registration->StartRegistration();
  }
  catch (itk::ExceptionObject & exception)
  {
    log << "image registration threw an exception: "
    << endl << exception.what() << endl;
  }
  t01->SetParameters(optimizer->GetBestParams());
  
  // evaluate the metric after the registration:
  double metric_after =
  eval_metric<metric_t, interpolator_t>(t01, fi, mi, fi_mask, mi_mask);
  
  translate_transform_t::ParametersType params_after =
  t01->GetParameters();
  
//  log << "BEFORE: " << metric_before << endl
//  << "AFTER:  " << metric_after << endl;
  
  if (metric_before <= metric_after || metric_after != metric_after)
  {
//    log << "NOTE: minimization failed, ignoring registration results..."
//    << endl;
    t01->SetParameters(params_before);
    metric_after = metric_before;
  }
  
  if (fn_prefix.size() != 0)
  {
    save_rgb<TImage>(fn_prefix + "b.tif", i0, i1, t01, m0, m1);
  }
  
  return metric_after;
}


//----------------------------------------------------------------
// refine_one_pair
// 
template <class TImage, class TMask>
double
refine_one_pair(/*the_log_t & log,*/
                
                const array2d(typename TImage::Pointer) & tile_pyramid,
                const array2d(typename TMask::ConstPointer) & mask_pyramid,
                const unsigned int & ia,
                const unsigned int & ib,
                translate_transform_t::Pointer & t01,
                
                const unsigned int iterations,
                const double & min_step,
                const double & max_step,
                
                const the_text_t & prefix = the_text_t(""))
{
  const unsigned int num_levels = tile_pyramid.size();
  
  double metric = std::numeric_limits<double>::max();
  for (unsigned int i = 0; i < num_levels; i++)
  {
    the_text_t fn_prefix;
    if (prefix.size() != 0)
    {
      fn_prefix = prefix + "level-" + the_text_t::number(i) + "-";
    }
    
    metric = refine_one_pair<TImage, TMask>
    (/*log,*/
     tile_pyramid[i][ia],
     tile_pyramid[i][ib],
     mask_pyramid[i][ia],
     mask_pyramid[i][ib],
     t01,
     iterations,
     min_step,
     max_step /* * integer_power<double>(2.0, num_levels - i - 1)*/,
     fn_prefix);
  }
  
  return metric;
}


//----------------------------------------------------------------
// refine_pairs
//
template <class TImage, class TMask>
void
refine_pairs(/*the_log_t & log,*/
             
             const std::vector<typename TImage::Pointer> & image,
             const std::vector<typename TMask::ConstPointer> & mask,
             const double & overlap_min,
             const double & overlap_max,
             const unsigned int & iterations,
             const double & min_step,
             const double & max_step,
             
             const array2d(translate_transform_t::Pointer) & mapping,
             array2d(translate_transform_t::Pointer) & path,
             array2d(double) & cost,
             
             const the_text_t & prefix = the_text_t(""))
{
  static unsigned int pass = 0;
  
  const unsigned int num_images = image.size();
  for (unsigned int i = 0; i < num_images - 1; i++)
  {
    for (unsigned int j = i + 1; j < num_images; j++)
    {
      // check whether the two images overlap:
      double overlap =
      overlap_ratio<TImage>(image[i], image[j], mapping[i][j]);
      if (overlap < overlap_min || overlap > overlap_max) continue;
      
      translate_transform_t::Pointer t_ij = translate_transform_t::New();
      t_ij->SetParameters(mapping[i][j]->GetParameters());
      
      log << "refining the mapping: " << setw(2) << j << " -> "
      << setw(2) << i << endl
      << "overlap: " << int(overlap * 100.0) << " percent" << endl;
      
      the_text_t fn_prefix;
      if (prefix.size() != 0)
      {
        fn_prefix =
        prefix +
        "pair-" +
        the_text_t::number(i, 2, '0') + "-" +
        the_text_t::number(j, 2, '0') + "-pass-" +
        the_text_t::number(pass, 2, '0') + "-";
      }
      
      cost[i][j] = refine_one_pair<TImage, TMask>(log,
                                                  image[i],
                                                  image[j],
                                                  mask[i],
                                                  mask[j],
                                                  t_ij,
                                                  iterations,
                                                  min_step,
                                                  max_step,
                                                  fn_prefix);
      
      overlap = overlap_ratio<TImage>(image[i], image[j], t_ij);
      
      if (overlap < overlap_min || overlap > overlap_max)
      {
        path[i][j] = 0;
        cost[i][j] = std::numeric_limits<double>::max();
      }
      else
      {
        // FIXME: encourage larger overlap:
        // cost[i][j] = 1.0 - overlap; // encourage edge overlap
        // cost[i][j] = overlap; // encourage corner overlap, works well !!!
        path[i][j] = t_ij;
      }
      
      path[j][i] = inverse(t_ij);
      cost[j][i] = cost[i][j];
    }
//    log << endl;
  }
  
//  int prev_precision = log.precision(2);
//  log << "\t";
//  for (unsigned int i = 0; i < num_images; i++) log << setw(9) << i;
//  log << endl;
//  log << "\t+-";
//  for (unsigned int i = 0; i < num_images; i++) log << "---------";
//  log << endl;
//  for (unsigned int i = 0; i < num_images; i++)
//  {
//    log << i << "\t| ";
//    for (unsigned int j = 0; j < num_images; j++)
//    {
//      if (path[i][j].GetPointer() != 0)
//      {
//        log << ' ' << cost[i][j];
//      }
//      else
//      {
//        log << "         ";
//      }
//    }
//    log << endl;
//  }
//  log << endl;
//  log.precision(prev_precision);
}

//----------------------------------------------------------------
// refine_pairs
// 
template <class TImage, class TMask>
void
refine_pairs(/*the_log_t & log,*/
             
             const array2d(typename TImage::Pointer) & tile_pyramid,
             const array2d(typename TMask::ConstPointer) & mask_pyramid,
             const double & overlap_min,
             const double & overlap_max,
             const unsigned int & iterations,
             const double & min_step,
             const double & max_step,
             
             const array2d(translate_transform_t::Pointer) & mapping,
             array2d(translate_transform_t::Pointer) & path,
             array2d(double) & cost,
             
             const the_text_t & prefix = the_text_t(""))
{
  static unsigned int pass = 0;
  pass++;
  
  unsigned int pyramid_levels = tile_pyramid.size();
  unsigned int high_res_level = pyramid_levels - 1;
  
  const unsigned int num_images = tile_pyramid[high_res_level].size();
  for (unsigned int i = 0; i < num_images - 1; i++)
  {
    for (unsigned int j = i + 1; j < num_images; j++)
    {
      // check whether the two images overlap:
      double overlap =
      overlap_ratio<TImage>(tile_pyramid[high_res_level][i],
                            tile_pyramid[high_res_level][j],
                            mapping[i][j]);
      if (overlap < overlap_min || overlap > overlap_max) continue;
      
      translate_transform_t::Pointer t_ij = translate_transform_t::New();
      t_ij->SetParameters(mapping[i][j]->GetParameters());
      
//      log << "refining the mapping: " << setw(2) << j << " -> "
//      << setw(2) << i << endl
//      << "overlap: " << int(overlap * 100.0) << " percent" << endl;
      
      the_text_t fn_prefix;
      if (prefix.size() != 0)
      {
        fn_prefix =
        prefix +
        "pair-" +
        the_text_t::number(i, 2, '0') + "-" +
        the_text_t::number(j, 2, '0') + "-pass-" +
        the_text_t::number(pass, 2, '0') + "-";
      }
      
      cost[i][j] = refine_one_pair<TImage, TMask>(/*log,*/
                                                  tile_pyramid,
                                                  mask_pyramid,
                                                  i,
                                                  j,
                                                  t_ij,
                                                  iterations,
                                                  min_step,
                                                  max_step,
                                                  fn_prefix);
      
      overlap = overlap_ratio<TImage>(tile_pyramid[high_res_level][i],
                                      tile_pyramid[high_res_level][j],
                                      t_ij);
      
      if (overlap < overlap_min || overlap > overlap_max)
      {
        path[i][j] = 0;
        cost[i][j] = std::numeric_limits<double>::max();
      }
      else
      {
        // FIXME: encourage larger overlap:
        // cost[i][j] = 1.0 - overlap; // encourage edge overlap
        // cost[i][j] = overlap; // encourage corner overlap, works well !!!
        path[i][j] = t_ij;
      }
      
      path[j][i] = inverse(t_ij);
      cost[j][i] = cost[i][j];
    }
    log << endl;
  }
  
//  int prev_precision = log.precision(2);
//  log << "\t";
//  for (unsigned int i = 0; i < num_images; i++) log << setw(9) << i;
//  log << endl;
//  log << "\t+-";
//  for (unsigned int i = 0; i < num_images; i++) log << "---------";
//  log << endl;
//  for (unsigned int i = 0; i < num_images; i++)
//  {
//    log << i << "\t| ";
//    for (unsigned int j = 0; j < num_images; j++)
//    {
//      if (path[i][j].GetPointer() != 0)
//      {
//        log << ' ' << cost[i][j];
//      }
//      else
//      {
//        log << "         ";
//      }
//    }
//    log << endl;
//  }
//  log << endl;
//  log.precision(prev_precision);
}


//----------------------------------------------------------------
// brute_force_one_pair
//
template <typename TImage, typename TMask>
double
brute_force_one_pair(const TImage * i0,
                     const TImage * i1,
                     const TMask * m0,
                     const TMask * m1,
                     translate_transform_t::Pointer & t01,
                     
                     const int & dx,
                     const int & dy,
                     
                     const the_text_t & fn_prefix = the_text_t(""))
{
  translate_transform_t::ParametersType init_params = t01->GetParameters();
  translate_transform_t::ParametersType best_params = init_params;
  translate_transform_t::ParametersType curr_params = init_params;
  
  translate_transform_t::Pointer t = translate_transform_t::New();
  double best_metric = std::numeric_limits<double>::max();
  typename TImage::SpacingType sp = i0->GetSpacing();
  
  typedef typename itk::NearestNeighborInterpolateImageFunction
  <TImage, double> nn_interpolator_t;
  typename nn_interpolator_t::Pointer nn = nn_interpolator_t::New();
  nn->SetInputImage(i1);
  
  if (fn_prefix.size() != 0)
  {
    save_rgb<TImage>(fn_prefix + "a.tif", i0, i1, t01, m0, m1);
  }
  
  for (int x = -dx; x <= dx; x++)
  {
    for (int y = -dy; y <= dy; y++)
    {
      // the window:
      curr_params[0] = init_params[0] + sp[0] * double(x);
      curr_params[1] = init_params[1] + sp[1] * double(y);
      
      t->SetParameters(curr_params);
      
      double overlap_area = 0;
      double metric =
      my_metric<TImage, nn_interpolator_t>
      (overlap_area, i0, i1, t, m0, m1, nn);
      
      if (metric < best_metric)
      {
        best_metric = metric;
        best_params = curr_params;
      }
    }
  }
  
  t01->SetParameters(best_params);
  
  if (fn_prefix.size() != 0)
  {
    save_rgb<TImage>(fn_prefix + "b.tif", i0, i1, t01, m0, m1);
  }
  
  return best_metric;
}


//----------------------------------------------------------------
// brute_force_one_pair
// 
template <typename TImage, typename TMask>
double
brute_force_one_pair
(const array2d(typename TImage::Pointer) & tile_pyramid,
 const array2d(typename TMask::ConstPointer) & mask_pyramid,
 const unsigned int & ia,
 const unsigned int & ib,
 translate_transform_t::Pointer & t01,
 
 const int & dx,
 const int & dy,
 const unsigned int & coarse_to_fine_levels,
 
 const the_text_t & prefix = the_text_t(""))
{
  unsigned int num_levels = std::min(coarse_to_fine_levels,
                                     (unsigned int)(tile_pyramid.size()));
  unsigned int high_res_level = num_levels - 1;
  
  for (unsigned int i = 0; i < num_levels; i++)
  {
    the_text_t fn_prefix;
    if (prefix.size() != 0)
    {
      fn_prefix = prefix + "level-" + the_text_t::number(i) + "-";
    }
    
    brute_force_one_pair<TImage, TMask>
    (tile_pyramid[i][ia].GetPointer(),
     tile_pyramid[i][ib].GetPointer(),
     mask_pyramid[i][ia].GetPointer(),
     mask_pyramid[i][ib].GetPointer(),
     t01,
     dx,
     dy,
     fn_prefix);
  }
  
  // setup the image-to-image metric:
  typedef itk::MeanSquaresImageToImageMetric<TImage, TImage> metric_t;
  
  typedef itk::LinearInterpolateImageFunction<TImage, double> interpolator_t;
  double metric =
  eval_metric<metric_t, interpolator_t>(t01,
                                        tile_pyramid[high_res_level][ia],
                                        tile_pyramid[high_res_level][ib],
                                        mask_pyramid[high_res_level][ia],
                                        mask_pyramid[high_res_level][ib]);
  return metric;
}


//----------------------------------------------------------------
// brute_force_pairs
//
template <typename TImage, typename TMask>
void
brute_force_pairs(/*the_log_t & log,*/
                  
                  const std::vector<typename TImage::Pointer> & image,
                  const std::vector<typename TMask::ConstPointer> & mask,
                  const double & overlap_min,
                  const double & overlap_max,
                  const int & dx,
                  const int & dy,
                  
                  const array2d(translate_transform_t::Pointer) & mapping,
                  array2d(translate_transform_t::Pointer) & path,
                  array2d(double) & cost,
                  
                  const the_text_t & prefix = the_text_t(""))
{
  static unsigned int pass = 0;
  pass++;
  
  const unsigned int num_images = image.size();
  for (unsigned int i = 0; i < num_images - 1; i++)
  {
    for (unsigned int j = i + 1; j < num_images; j++)
    {
      // check whether the two images overlap:
      double overlap =
      overlap_ratio<TImage>(image[i], image[j], mapping[i][j]);
      if (overlap < overlap_min || overlap > overlap_max) continue;
      
      translate_transform_t::Pointer t_ij = translate_transform_t::New();
      t_ij->SetParameters(mapping[i][j]->GetParameters());
      
//      log << "refining the mapping: " << setw(2) << j << " -> "
//      << setw(2) << i << endl
//      << "overlap: " << int(overlap * 100.0) << " percent" << endl;
      
      the_text_t fn_prefix;
      if (prefix.size() != 0)
      {
        fn_prefix =
        prefix +
        "pair-" +
        the_text_t::number(i, 2, '0') + "-" +
        the_text_t::number(j, 2, '0') + "-pass-" +
        the_text_t::number(pass, 2, '0') + "-";
      }
      
      cost[i][j] = brute_force_one_pair<TImage, TMask>(image[i],
                                                       image[j],
                                                       mask[i],
                                                       mask[j],
                                                       t_ij,
                                                       dx,
                                                       dy,
                                                       fn_prefix);
      
      overlap = overlap_ratio<TImage>(image[i], image[j], t_ij);
      
      if (overlap < overlap_min || overlap > overlap_max)
      {
        path[i][j] = 0;
        cost[i][j] = std::numeric_limits<double>::max();
      }
      else
      {
        // FIXME: encourage larger overlap:
        // cost[i][j] = 1.0 - overlap; // encourage edge overlap
        // cost[i][j] = overlap; // encourage corner overlap, works well !!!
        path[i][j] = t_ij;
      }
      
      path[j][i] = inverse(t_ij);
      cost[j][i] = cost[i][j];
    }
//    log << endl;
  }
  
//  int prev_precision = log.precision(2);
//  log << "\t";
//  for (unsigned int i = 0; i < num_images; i++) log << setw(9) << i;
//  log << endl;
//  log << "\t+-";
//  for (unsigned int i = 0; i < num_images; i++) log << "---------";
//  log << endl;
//  for (unsigned int i = 0; i < num_images; i++)
//  {
//    log << i << "\t| ";
//    for (unsigned int j = 0; j < num_images; j++)
//    {
//      if (path[i][j].GetPointer() != 0)
//      {
//        log << ' ' << cost[i][j];
//      }
//      else
//      {
//        log << "         ";
//      }
//    }
//    log << endl;
//  }
//  log << endl;
//  log.precision(prev_precision);
}

//----------------------------------------------------------------
// brute_force_pairs
// 
template <typename TImage, typename TMask>
void
brute_force_pairs(/*the_log_t & log,*/
                  
                  const array2d(typename TImage::Pointer) & tile_pyramid,
                  const array2d(typename TMask::ConstPointer) & mask_pyramid,
                  const double & overlap_min,
                  const double & overlap_max,
                  const int & dx,
                  const int & dy,
                  
                  // number of levels to try, starting from the lowest:
                  const unsigned int & coarse_to_fine_levels,
                  
                  const array2d(translate_transform_t::Pointer) & mapping,
                  array2d(translate_transform_t::Pointer) & path,
                  array2d(double) & cost,
                  
                  const the_text_t & prefix = the_text_t(""))
{
  static unsigned int pass = 0;
  pass++;
  
  unsigned int pyramid_levels = tile_pyramid.size();
  unsigned int high_res_level = pyramid_levels - 1;
  
  const unsigned int num_images = tile_pyramid[high_res_level].size();
  for (unsigned int i = 0; i < num_images - 1; i++)
  {
    for (unsigned int j = i + 1; j < num_images; j++)
    {
      // check whether the two images overlap:
      double overlap =
      overlap_ratio<TImage>(tile_pyramid[high_res_level][i],
                            tile_pyramid[high_res_level][j],
                            mapping[i][j]);
      if (overlap < overlap_min || overlap > overlap_max) continue;
      
      translate_transform_t::Pointer t_ij = translate_transform_t::New();
      t_ij->SetParameters(mapping[i][j]->GetParameters());
      
//      log << "refining the mapping: " << setw(2) << j << " -> "
//      << setw(2) << i << endl
//      << "overlap: " << int(overlap * 100.0) << " percent" << endl;
      
      the_text_t fn_prefix;
      if (prefix.size() != 0)
      {
        fn_prefix =
        prefix +
        "pair-" +
        the_text_t::number(i, 2, '0') + "-" +
        the_text_t::number(j, 2, '0') + "-pass-" +
        the_text_t::number(pass, 2, '0') + "-";
      }
      
      cost[i][j] = brute_force_one_pair<TImage, TMask>(tile_pyramid,
                                                       mask_pyramid,
                                                       i,
                                                       j,
                                                       t_ij,
                                                       dx,
                                                       dy,
                                                       coarse_to_fine_levels,
                                                       fn_prefix);
      
      overlap = overlap_ratio<TImage>(tile_pyramid[high_res_level][i],
                                      tile_pyramid[high_res_level][j],
                                      t_ij);
      
      if (overlap < overlap_min || overlap > overlap_max)
      {
        path[i][j] = 0;
        cost[i][j] = std::numeric_limits<double>::max();
      }
      else
      {
        // FIXME: encourage larger overlap:
        // cost[i][j] = 1.0 - overlap; // encourage edge overlap
        // cost[i][j] = overlap; // encourage corner overlap, works well !!!
        path[i][j] = t_ij;
      }
      
      path[j][i] = inverse(t_ij);
      cost[j][i] = cost[i][j];
    }
//    log << endl;
  }
  
//  int prev_precision = log.precision(2);
//  log << "\t";
//  for (unsigned int i = 0; i < num_images; i++) log << setw(9) << i;
//  log << endl;
//  log << "\t+-";
//  for (unsigned int i = 0; i < num_images; i++) log << "---------";
//  log << endl;
//  for (unsigned int i = 0; i < num_images; i++)
//  {
//    log << i << "\t| ";
//    for (unsigned int j = 0; j < num_images; j++)
//    {
//      if (path[i][j].GetPointer() != 0)
//      {
//        log << ' ' << cost[i][j];
//      }
//      else
//      {
//        log << "         ";
//      }
//    }
//    log << endl;
//  }
//  log << endl;
//  log.precision(prev_precision);
}


//----------------------------------------------------------------
// dump_neighbors
// 
template <typename TImage, typename TMask>
void
dump_neighbors(/*the_log_t & log,*/
               
               const std::vector<typename TImage::Pointer> & image,
               const std::vector<typename TMask::ConstPointer> & mask,
               const array2d(translate_transform_t::Pointer) & path,
               const the_text_t & prefix)
{
  static unsigned int pass = 0;
  pass++;
  
  const unsigned int num_images = image.size();
  for (unsigned int i = 0; i < num_images; i++)
  {
    std::list<typename TImage::Pointer> image_list;
    std::list<typename TMask::ConstPointer> mask_list;
    std::list<base_transform_t::ConstPointer> transform_list;
    
    image_list.push_back(image[i]);
    mask_list.push_back(mask[i]);
    transform_list.push_back((identity_transform_t::New()).GetPointer());
    
//    log << endl;
    for (unsigned int j = 0; j < num_images; j++)
    {
      if (i == j) continue;
      if (path[i][j].GetPointer() == 0) continue;
      
//      log << "found a mapping: " << setw(2) << j << " -> "
//      << setw(2) << i << endl;
      
      image_list.push_back(image[j]);
      mask_list.push_back(mask[j]);
      transform_list.push_back(path[i][j].GetPointer());
    }
    
    std::vector<typename TImage::Pointer>
    images(image_list.begin(), image_list.end());
    
    std::vector<typename TMask::ConstPointer>
    masks(mask_list.begin(), mask_list.end());
    
    std::vector<base_transform_t::ConstPointer>
    transforms(transform_list.begin(), transform_list.end());
    
    typename TImage::Pointer mosaic[3];
    make_mosaic_rgb(mosaic,
                    std::vector<bool>(images.size(), false),
                    transforms,
                    images,
                    masks,
                    FEATHER_NONE_E,
                    255,
                    true);
    
    the_text_t fn_save =
    prefix + "neighbors-" +
    the_text_t::number(i, 2, '0') +
    "-pass-" +
    the_text_t::number(pass) +
    ".png";
    save_rgb<typename TImage::Pointer>(mosaic, fn_save, true);
  }
}


//----------------------------------------------------------------
// calc_area_and_dist
//
template <class TImage, class transform_ptr_t>
void
calc_area_and_dist(const std::vector<typename TImage::Pointer> & image,
                   const array2d(transform_ptr_t) & mapping,
                   array2d(double) & overlap,
                   array2d(double) & distance)
{
  const unsigned int num_images = image.size();
  resize(overlap, num_images, num_images);
  resize(distance, num_images, num_images);
  
  for (unsigned int i = 0; i < num_images; i++)
  {
    if (image[i].GetPointer() == 0) continue;
    
    // calculate the tile center:
    pnt2d_t ci;
    {
      typename TImage::SizeType sz =
      image[i]->GetLargestPossibleRegion().GetSize();
      
      typename TImage::SpacingType sp =
      image[i]->GetSpacing();
      
      pnt2d_t tile_min = image[i]->GetOrigin();
      pnt2d_t tile_max;
      tile_max[0] = tile_min[0] + sp[0] * double(sz[0]);
      tile_max[1] = tile_min[1] + sp[1] * double(sz[1]);
      
      ci = pnt2d(tile_min[0] + (tile_max[0] - tile_min[0]) / 2.0,
                 tile_min[1] + (tile_max[1] - tile_min[1]) / 2.0);
    }
    
    overlap[i][i] = 1.0;
    distance[i][i] = 0.0;
    
    for (unsigned int j = i + 1; j < num_images; j++)
    {
      if (mapping[i][j].GetPointer() == 0) continue;
      overlap[i][j] = overlap_ratio<TImage>(image[i], image[j],
                                            mapping[i][j]);
      overlap[j][i] = overlap[i][j];
      
      pnt2d_t cj;
      {
        typename TImage::SizeType sz =
        image[j]->GetLargestPossibleRegion().GetSize();
        
        typename TImage::SpacingType sp =
        image[j]->GetSpacing();
        
        pnt2d_t tile_min = image[j]->GetOrigin();
        pnt2d_t tile_max;
        tile_max[0] = tile_min[0] + sp[0] * double(sz[0]);
        tile_max[1] = tile_min[1] + sp[1] * double(sz[1]);
        
        pnt2d_t center =
        pnt2d(tile_min[0] + (tile_max[0] - tile_min[0]) / 2.0,
              tile_min[1] + (tile_max[1] - tile_min[1]) / 2.0);
        
        cj = mapping[j][i]->TransformPoint(center);
      }
      
      distance[i][j] = (ci - cj).GetNorm();
      distance[j][i] = distance[i][j];
    }
  }
}

//
////----------------------------------------------------------------
//// local_mapping_t
//// 
//class local_mapping_t
//{
//public:
//  local_mapping_t(const unsigned int & from = UINT_MAX,
//                  const unsigned int & to = UINT_MAX,
//                  const double & dist = std::numeric_limits<double>::max()):
//  from_(from),
//  to_(to),
//  distance_(dist)
//  {}
//  
//  inline bool operator < (const local_mapping_t & m) const
//  {
//    if (to_ == m.to_) return distance_ < m.distance_;
//    return to_ < m.to_;
//  }
//  
//  inline bool operator == (const local_mapping_t & m) const
//  { return to_ == m.to_; }
//  
//  unsigned int from_;
//  unsigned int to_;
//  double distance_;
//};
//
//
//
////----------------------------------------------------------------
//// neighbor_t
// 
//class neighbor_t
//{
//public:
//  neighbor_t():
//  id_(UINT_MAX),
//  metric_(std::numeric_limits<double>::max())
//  {}
//  
//  neighbor_t(const unsigned int & id,
//             const double & metric,
//             translate_transform_t::Pointer & t):
//  id_(id),
//  metric_(metric),
//  t_(t)
//  {}
//  
//  inline bool operator == (const neighbor_t & d) const
//  { return id_ == d.id_; }
//  
//  inline bool operator < (const neighbor_t & d) const
//  { return metric_ < d.metric_; }
//  
//  unsigned int id_;
//  double metric_;
//  translate_transform_t::Pointer t_;
//};

//----------------------------------------------------------------
// match_pairs
//
template <class TImage, class TMask>
bool
match_pairs(/*the_log_t & log,*/
            
            std::vector<typename TImage::Pointer> & image,
            std::vector<typename TMask::ConstPointer> & mask,
            const bool & images_were_resampled,
            const bool & use_std_mask,
            
            std::vector<bool> & matched,
            array2d(translate_transform_t::Pointer) & path,
            array2d(double) & cost,
            
            const double & overlap_min,
            const double & overlap_max,
            
            image_t::PointType offset_min,
            image_t::PointType offset_max,
            
            // ideally this should be one, but radial distortion may
            // generate several valid peaks (up to 4), so it may be
            // necessary to consider more peaks for the unmatched images:
            const unsigned int max_peaks = 1)
{
  // shortcut:
  const unsigned int num_images = image.size();
  if (num_images < 2) return false;
  
  // initialize the mappings and mapping costs:
  for (unsigned int i = 0; i < num_images; i++)
  {
    matched[i] = false;
    for (unsigned int j = 0; j < num_images; j++)
    {
      path[i][j] = 0;
      cost[i][j] = std::numeric_limits<double>::max();
    }
  }
  
  // this map is used to match the mismatched images:
  // index  = image id
  // first  = neighbor image id
  // second = list of peaks in the corresponding displacement PDF
  std::vector<std::pair<unsigned int, std::list<local_max_t> > >
  peaks(num_images);
  
  std::list<unsigned int> perimeter;
  bool perimeter_seeded = false;
  for (unsigned int seed = 0;
       seed < num_images && (!perimeter_seeded || !perimeter.empty());
       seed++)
  {
    // bootstrap the perimeter:
    if (!perimeter_seeded) push_back_unique(perimeter, seed);
    
    // try to match the perimeter to the unmatched images; as new matches
    // are discovered they are added to the perimeter:
    while (!perimeter.empty())
    {
//      log << "- - - - - - - - - - - - - - - - - - - - - - - - - - -" << endl;
      
      // consider a node from the perimeter:
      unsigned int node_id = remove_head(perimeter);
      matched[node_id] = true;
      
      // match it with the remaining unmatched nodes:
      std::list<neighbor_t> md;
      unsigned int md_size = 0;
      for (unsigned int i = 0; i < matched.size(); i++)
      {
        if (matched[i]) continue;
        
        log << endl << "matching image " << node_id << " to " << i << ", "
        << flush;
        
        const TImage * fi = image[node_id];
        const TImage * mi = image[i];
        const TMask * fi_mask = mask[node_id];
        const TMask * mi_mask = mask[i];
        
        neighbor_t best_match;
        best_match.id_ = i;
        best_match.metric_ =
        match_one_pair<TImage, TMask>(/*log,*/
                                      images_were_resampled,
                                      use_std_mask,
                                      fi,
                                      mi,
                                      fi_mask,
                                      mi_mask,
                                      overlap_min,
                                      overlap_max,
                                      offset_min,
                                      offset_max,
                                      node_id,
                                      best_match.t_,
                                      peaks[i],
                                      max_peaks);
        
        if (best_match.metric_ != std::numeric_limits<double>::max())
        {
          md.push_back(best_match);
          md_size++;
        }
      }
      
      // pick the top few matches from the sea of possible matches:
      md.sort();
      
      std::list<unsigned int> next_perimeter;
      
//      if (md_size != 0)
//      {
//        log << endl << "METRICS: " << flush;
//      }
//      else
//      {
//        log << endl << "WARNING: " << node_id
//        << " did not match any other tile" << flush;
//      }
      
      bool separator = false;
      for (std::list<neighbor_t>::iterator i = md.begin();
           i != md.end(); ++i)
      {
        const neighbor_t & best = *(md.begin());
        const neighbor_t & data = *i;
        
        double decay = (1.0 + data.metric_) / (1.0 + best.metric_);
        if (decay > 4.0)
        {
          if (!separator)
          {
//            log << ".... ";
            separator = true;
          }
        }
        else
        {
          next_perimeter.push_front(data.id_);
          
          path[node_id][data.id_] = data.t_;
          cost[node_id][data.id_] = data.metric_;
          path[data.id_][node_id] = inverse(data.t_);
          cost[data.id_][node_id] = data.metric_;
        }
        
//        log << data.metric_ << ' ' << flush;
      }
//      log << endl;
      
      // expand the perimeter (elements must remain unique):
      expand(perimeter, next_perimeter, true);
      
      // clear the perimeter seeding flag:
      perimeter_seeded = perimeter_seeded || (next_perimeter.size() != 0);
    }
    
    // don't try to match the unmatched images
    // until we have a decent perimeter:
    if (!perimeter_seeded) continue;
    
    // try to match the unmatched images:
    for (unsigned int i = 0; i < num_images; i++)
    {
      if (matched[i]) continue;
      
      unsigned int node_id = peaks[i].first;
      const TImage * fi = image[node_id];
      const TImage * mi = image[i];
      const TMask * fi_mask = mask[node_id];
      const TMask * mi_mask = mask[i];
      
      // choose the best peak:
      neighbor_t best_match;
      for (std::list<local_max_t>::iterator j = peaks[i].second.begin();
           j != peaks[i].second.end(); ++j)
      {
        log << "evaluating permutations..." << endl;
        const local_max_t & lm = *j;
        
        image_t::PointType offset_min;
        offset_min[0] = -std::numeric_limits<double>::max();
        offset_min[1] = -std::numeric_limits<double>::max();
        image_t::PointType offset_max;
        offset_max[0] = std::numeric_limits<double>::max();
        offset_max[1] = std::numeric_limits<double>::max();
        
        translate_transform_t::Pointer ti = translate_transform_t::New();
        double metric = estimate_displacement<TImage>(/*log,*/
                                                      fi,
                                                      mi,
                                                      lm,
                                                      ti,
                                                      offset_min,
                                                      offset_max,
                                                      overlap_min,
                                                      overlap_max,
                                                      fi_mask,
                                                      mi_mask);
        if (metric > best_match.metric_) continue;
        
        best_match = neighbor_t(i, metric, ti);
      }
//      log << endl;
      if (best_match.id_ == UINT_MAX) continue;
//      log << "METRIC: " << best_match.metric_ << endl;
      
      perimeter.push_front(best_match.id_);
      
      path[node_id][best_match.id_] = best_match.t_;
      cost[node_id][best_match.id_] = best_match.metric_;
      path[best_match.id_][node_id] = inverse(best_match.t_);
      cost[best_match.id_][node_id] = best_match.metric_;
    }
  }
  
  return perimeter_seeded;
}


//----------------------------------------------------------------
// layout_mosaic
// 
template <class TImage, class TMask>
void
layout_mosaic
(/*the_log_t & log,*/
 
 // multi-resolution image tiles and tile masks,
 // the pyramids are sorted low-res first to high-res last:
 const unsigned int & num_tiles,
 array2d(typename TImage::Pointer) & tile_pyramid,
 array2d(typename TMask::ConstPointer) & mask_pyramid,
 
 // maximum number of intermediate tiles in cascaded mappings:
 const unsigned int & max_cascade_len,
 
 // the final transforms associated with each tile:
 std::vector<affine_transform_t::Pointer> & affine,
 
 // a vector identifying which tiles have been successfully matched:
 std::vector<bool> & matched,
 
 // matched tile layout order:
 std::list<unsigned int> & tile_order,
 
 // cost[num_intermediate_tiles][i][j] contains the maximum error metric
 // due to the image mapping between each image in the cascading chain:
 array3d(double) & cost,
 
 // path[num_intermediate_tiles][i][j] contains the transforms
 // associated with the specified number of intermediate images
 // that maps image i to image j:
 array3d(translate_transform_t::Pointer) & path,
 
 // image matching parameters:
 bool images_were_resampled,
 bool use_std_mask,
 
 // min/max tile overlap parameters:
 double overlap_min,
 double overlap_max,
 
 image_t::PointType offset_min,
 image_t::PointType offset_max,
 
 unsigned int max_peaks,
 
 bool try_refining = false,
 const the_text_t & prefix = the_text_t(""))
{
  typedef itk::LinearInterpolateImageFunction<TImage, double> interpolator_t;
  
  static const unsigned int iterations_per_level = 8;
  unsigned int pyramid_levels = tile_pyramid.size();
  unsigned int high_res_level = pyramid_levels - 1;
  
  bool perimeter_seeded =
  match_pairs<TImage, TMask>(/*log,*/
                             tile_pyramid[high_res_level],
                             mask_pyramid[high_res_level],
                             images_were_resampled,
                             use_std_mask,
                             matched,
                             path[0],
                             cost[0],
                             overlap_min,
                             overlap_max,
                             offset_min,
                             offset_max,
                             max_peaks);
  
  if (!perimeter_seeded)
  {
//    log << "WARNING: none of the tiles matched..." << endl;
    return;
  }
  
  // try to improve the mappings as much as possible:
  for (unsigned int i = 0; i < 1; i++)
  {
    // assemble cascaded transform chains with a prescribed maximum number of
    // intermediate nodes, try to choose the chain that is the most stable:
    assemble_cascades(num_tiles, max_cascade_len, path, cost, false);
    
    // collapse the graph into a mapping:
    array2d(translate_transform_t::Pointer) mapping;
    resize(mapping, num_tiles, num_tiles);
    
    array2d(double) mapping_cost;
    resize(mapping_cost, num_tiles, num_tiles);
    
    establish_mappings(/*log,*/
                       num_tiles,
                       max_cascade_len,
                       path,
                       cost,
                       mapping,
                       mapping_cost);
    
    reset(num_tiles, max_cascade_len, path, cost);
    
    // for each image, determine its immediate neighbors and establish
    // refined direct mappings to them:
#if 1
    brute_force_pairs<TImage, TMask>(/*log,*/
                                     tile_pyramid,
                                     mask_pyramid,
                                     overlap_min,
                                     overlap_max,
                                     7, // dx
                                     7, // dy
                                     2, // pyramid_levels, // refinement levels
                                     mapping,
                                     path[0],
                                     cost[0],
                                     prefix);
#else
    refine_pairs<TImage, TMask>(log,
                                tile_pyramid,
                                mask_pyramid,
                                overlap_min,
                                overlap_max,
                                iterations_per_level,
                                1e-6, // min_step
                                1e-0, // max_step
                                mapping,
                                path[0],
                                cost[0],
                                prefix);
#endif
    
    if (prefix.size() != 0)
    {
      dump_neighbors<TImage, TMask>(/*log,*/
                                    tile_pyramid[high_res_level],
                                    mask_pyramid[high_res_level],
                                    path[0],
                                    prefix);
    }
  }
  
  // find local overlaps and distances between overlapping tiles:
  array2d(double) local_overlap;
  array2d(double) local_distance;
  calc_area_and_dist<TImage, translate_transform_t::Pointer>
  (tile_pyramid[high_res_level], path[0], local_overlap, local_distance);
  
  unsigned int best_target = ~0;
  
#if 1
  double best_overlap = 0.0;
  for (unsigned int i = 0; i < num_tiles; i++)
  {
    double overlap_sum = 0.0;
    for (unsigned int j = 0; j < num_tiles; j++)
    {
      if (j == i) continue;
      
      overlap_sum += local_overlap[i][j];
    }
    
    if (overlap_sum > best_overlap)
    {
      best_target = i;
      best_overlap = overlap_sum;
    }
  }
#else
  double worst_metric = 0.0;
  for (unsigned int i = 0; i < num_tiles; i++)
  {
    double metric_sum = 0.0;
    double weight = 0.0;
    for (unsigned int j = 0; j < num_tiles; j++)
    {
      if (j == i) continue;
      
      metric_sum += cost[0][i][j];
      weight += 1.0;
    }
    
    if (weight != 0.0)
    {
      metric_sum /= weight;
    }
    
    if (metric_sum > worst_metric)
    {
      best_target = i;
      worst_metric = metric_sum;
    }
  }
#endif
  
  // sort the tiles according to their distance to the target tile:
  std::list<local_mapping_t> perimeter;
  perimeter.push_back(local_mapping_t(best_target, best_target, 0.0));
  
  // setup a list of remaining tiles:
  std::list<unsigned int> tiles;
  for (unsigned int i = 0; i < num_tiles; i++)
  {
    if (i == best_target) continue;
    if (tile_pyramid[high_res_level][i].GetPointer() == 0) continue;
    
    tiles.push_back(i);
  }
  
  // setup the initial transform:
  affine.resize(num_tiles);
  {
    identity_transform_t::Pointer identity = identity_transform_t::New();
    affine[best_target] =
    setup_transform<TImage>(tile_pyramid[high_res_level][best_target],
                            identity);
  }
  
  // setup the metric transform parameter masks:
  std::vector<bool> param_shared;
  std::vector<bool> param_active;
  affine_transform_t::setup_shared_params_mask(false, param_shared);
  param_active.assign(param_shared.size(), true);
  
  // layout and refine the mosaic incrementally as a flood fill:
  tile_order.clear();
  while (true)
  {
    std::list<local_mapping_t> candidates;
    
    // look at the current perimeter and generate the next perimeter:
    while (!perimeter.empty())
    {
      const unsigned int i = remove_head(perimeter).to_;
      tile_order.push_back(i);
      
      // look at the remaining tiles as the candidates for the next perimeter:
      for (std::list<unsigned int>::const_iterator jter = tiles.begin();
           jter != tiles.end(); ++jter)
      {
        const unsigned int j = *jter;
        if (path[0][i][j].GetPointer() == 0) continue;
        
        local_mapping_t m(i, j, cost[0][i][j]);
        candidates.push_back(m);
      }
    }
    
    if (candidates.empty())
    {
      // done:
      break;
    }
    
    // remove duplicate mappings to the same tile, assemble the next perimeter:
    candidates.sort();
    perimeter.push_back(remove_head(candidates));
    tiles.remove(perimeter.back().to_);
    
    while (!candidates.empty())
    {
      const local_mapping_t & prev = perimeter.back();
      local_mapping_t next = remove_head(candidates);
      if (prev.to_ == next.to_) continue;
      
      perimeter.push_back(next);
      tiles.remove(next.to_);
    }
    
    // number of tiles that will be optimized in the next step:
    const unsigned int num_optimize = tile_order.size() + perimeter.size();
    
    // try incremental refinement of the mosaic:
    typedef itk::ImageMosaicVarianceMetric<TImage, interpolator_t>
    mosaic_metric_t;
    
    typename mosaic_metric_t::Pointer mosaic_metric = mosaic_metric_t::New();
    mosaic_metric->image_.resize(num_optimize);
    mosaic_metric->mask_.resize(num_optimize);
    mosaic_metric->transform_.resize(num_optimize);
    
    std::vector<unsigned int> tile_id(num_optimize);
    
    unsigned int i = 0;
    for (std::list<unsigned int>::const_iterator iter = tile_order.begin();
         iter != tile_order.end(); ++iter, i++)
    {
      tile_id[i] = *iter;
      mosaic_metric->image_[i] = tile_pyramid[high_res_level][tile_id[i]];
      mosaic_metric->mask_[i] = mask_pyramid[high_res_level][tile_id[i]];
      mosaic_metric->transform_[i] = affine[tile_id[i]].GetPointer();
    }
    
    for (std::list<local_mapping_t>::const_iterator iter = perimeter.begin();
         iter != perimeter.end(); ++iter, i++)
    {
      const local_mapping_t & m = *iter;
      tile_id[i] = m.to_;
      affine[tile_id[i]] =
      setup_transform<TImage>(tile_pyramid[high_res_level][tile_id[i]],
                              affine[m.from_],
                              path[0][m.from_][tile_id[i]]);
      
      mosaic_metric->image_[i] = tile_pyramid[high_res_level][tile_id[i]];
      mosaic_metric->mask_[i] = mask_pyramid[high_res_level][tile_id[i]];
      mosaic_metric->transform_[i] = affine[tile_id[i]].GetPointer();
    }
    
    if (!try_refining)
    {
      continue;
    }
    
    mosaic_metric->setup_param_map(param_shared, param_active);
    mosaic_metric->Initialize();
    
    // setup the optimizer scales:
    typename mosaic_metric_t::params_t parameter_scales = 
    mosaic_metric->GetTransformParameters();
    parameter_scales.Fill(1.0);
    
    // discourage skewing and rotation:
    for (unsigned int k = 0; k < num_optimize; k++)
    {
      const unsigned int * addr = &(mosaic_metric->address_[k][0]);
      
      parameter_scales[addr[affine_transform_t::index_a(0, 1)]] = 1e+6;
      parameter_scales[addr[affine_transform_t::index_b(1, 0)]] = 1e+6;
    }
    
    for (unsigned int i = 0; i < num_optimize; i++)
    {
      mosaic_metric->image_[i] = tile_pyramid[high_res_level][tile_id[i]];
    }
    
    typename mosaic_metric_t::measure_t metric_before =
    mosaic_metric->GetValue(mosaic_metric->GetTransformParameters());
    
    // run several iterations of the optimizer:
    for (unsigned int k = 0; k < 1; k++)
    {
      typename mosaic_metric_t::params_t params_before = 
      mosaic_metric->GetTransformParameters();
      
      typename mosaic_metric_t::measure_t metric_after =
      std::numeric_limits<typename mosaic_metric_t::measure_t>::max();
      
      // use global refinement:
      optimizer_t::Pointer optimizer = optimizer_t::New();
      optimizer_observer_t<optimizer_t>::Pointer observer =
      optimizer_observer_t<optimizer_t>::New();
//      observer->log_ = &log;
      optimizer->AddObserver(itk::IterationEvent(), observer);
//      optimizer->SetLog(&log);
      optimizer->SetMinimize(true);
      optimizer->SetNumberOfIterations(iterations_per_level);
      optimizer->SetMinimumStepLength(1e-12);        // min_step
      optimizer->SetMaximumStepLength(1e-5);        // max_step
      optimizer->SetGradientMagnitudeTolerance(1e-6);
      optimizer->SetRelaxationFactor(5e-1);
      optimizer->SetCostFunction(mosaic_metric);
      optimizer->SetInitialPosition(params_before);
      optimizer->SetScales(parameter_scales);
      optimizer->SetPickUpPaceSteps(5);
      optimizer->SetBackTracking(true);
      
      // refine the mosaic:
      try
      {
//        log << "\n" << k << ": refining affine transforms"
//        << endl;
        
        optimizer->StartOptimization();
      }
      catch (itk::ExceptionObject & exception)
      {
        // oops:
//        log << "optimizer threw an exception:" << endl
//        << exception.what()
//        << endl;
      }
      mosaic_metric->SetTransformParameters(optimizer->GetBestParams());
      metric_after = optimizer->GetBestValue();
      
      typename mosaic_metric_t::params_t params_after = 
      mosaic_metric->GetTransformParameters();
      
//      log << "before: METRIC = " << metric_before
//      << ", PARAMS = " << params_before << endl
//      << "after:  METRIC = " << metric_after
//      << ", PARAMS = " << params_after << endl;
      
      // quantify the improvement:
      double improvement = 1.0 - metric_after / metric_before;
      bool failed_to_improve = (metric_after - metric_before) >= 0.0;
      bool negligible_improvement = !failed_to_improve && (improvement < 1e-3);
      
//      if (!failed_to_improve)
//      {
//        log << "IMPROVEMENT: " << setw(3) << int(100.0 * improvement)
//        << "%" << endl;
//      }
      
      if (failed_to_improve)
      {
//        log << "NOTE: minimization failed, ignoring registration results..."
//        << endl;
        
        // previous transform was better:
        mosaic_metric->SetTransformParameters(params_before);
        break;
      }
      else if (negligible_improvement)
      {
//        log << "NOTE: improvement is negligible..." << endl;
        break;
      }
      
      // avoid recalculating the same metric:
      metric_before = metric_after;
    }
  }
}

#endif