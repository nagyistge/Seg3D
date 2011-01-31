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

#ifndef APPLICATION_PROVENANCE_PROVENANCESTEP_H
#define APPLICATION_PROVENANCE_PROVENANCESTEP_H

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
#pragma once
#endif

namespace Seg3D
{

// PROVENANCEID:
// The full provenance database is organized by provenance IDs. These are ids are unique numbers
// used in the provenance database to store provenance records.
typedef long long ProvenanceID;
typedef std::vector<ProvenanceID> ProvenanceIDList;

// GENERATEPROVENANCEID:
// Generate a new provenance ID
ProvenanceID GenerateProvenanceID();

// GETPROVENANCECOUNT:
// Retrieve the current value from the provenance counter
ProvenanceID GetProvenanceCount();

// SETPROVENANCECOUNT:
// Set the current value of the provenance counter
void SetProvenanceCount( ProvenanceID count );

class ProvenanceStepPrivate;

class ProvenanceStep
{

public:
	ProvenanceStep( const ProvenanceIDList& inputs, const std::string& action );
	virtual ~ProvenanceStep();
	
public:
	ProvenanceID get_provenance_id() const;
	std::string get_action() const;
	std::string get_user() const;
	bool get_inputs( ProvenanceIDList& provenance_vector ) const;
	void set_inputs(  ProvenanceIDList inputs  );

private:
	boost::shared_ptr< ProvenanceStepPrivate > private_;

};

} // end namespace Seg3D

#endif //APPLICATION_PROVENANCE_PROVENANCESTEP_H
