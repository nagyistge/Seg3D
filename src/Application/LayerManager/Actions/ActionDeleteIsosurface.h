/*
 For more information, please see: http://software.sci.utah.edu
 
 The MIT License
 
 Copyright (c) 2009 Scientific Computing and Imaging Institute,
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

#ifndef APPLICATION_LAYERMANAGER_ACTIONS_ACTIONDELETEISOSURFACE_H
#define APPLICATION_LAYERMANAGER_ACTIONS_ACTIONDELETEISOSURFACE_H

// Core includes
#include <Core/Action/Actions.h>
#include <Core/Interface/Interface.h>

// Application includes
#include <Application/Layer/LayerFWD.h>

namespace Seg3D
{

class ActionDeleteIsosurface : public Core::Action
{

CORE_ACTION( 
  CORE_ACTION_TYPE( "DeleteIsosurface", "Delete isosurface for the selected layer.")
  CORE_ACTION_ARGUMENT( "layerid", "The layerid of the layer for which the isosurface should be deleted." )
  CORE_ACTION_CHANGES_PROJECT_DATA()
)
  
  // -- Constructor/Destructor --
public:
  ActionDeleteIsosurface()
  {
    this->add_argument( this->layer_id_ );
  }
  
  virtual ~ActionDeleteIsosurface()
  {
  }
  
// -- Functions that describe action --
public:
  virtual bool validate( Core::ActionContextHandle& context );
  virtual bool run( Core::ActionContextHandle& context, Core::ActionResultHandle& result );

private:
  // This parameter contains the id of the layer group
  Core::ActionParameter< std::string > layer_id_;
  
  // -- Dispatch this action from the interface --
public:

  // CREATE:
  // Create an action that deletes the isosurface for the selected layer
  static Core::ActionHandle Create( MaskLayerHandle mask_layer );

  // DISPATCH
  // Create and dispatch action that deletes the isosurface for the selected layer
  static void Dispatch( Core::ActionContextHandle context, MaskLayerHandle mask_layer );  
};
  
} // end namespace Seg3D

#endif
