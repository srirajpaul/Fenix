<pre>
/*
// ************************************************************************
//
//
//            _|_|_|_|  _|_|_|_|  _|      _|  _|_|_|  _|      _|
//            _|        _|        _|_|    _|    _|      _|  _|
//            _|_|_|    _|_|_|    _|  _|  _|    _|        _|
//            _|        _|        _|    _|_|    _|      _|  _|
//            _|        _|_|_|_|  _|      _|  _|_|_|  _|      _|
//
//
//
//
// Copyright (C) 2016 Rutgers University and Sandia Corporation
//
// Under the terms of Contract DE-AC04-94AL85000 with Sandia Corporation,
// the U.S. Government retains certain rights in this software.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// 1. Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// 3. Neither the name of the Corporation nor the names of the
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY RUTGERS UNIVERSITY AND SANDIA 
// CORPORATION "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, 
// BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND 
// FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL 
// RUTGERS UNIVERSITY, SANDIA CORPORATION OR THE CONTRIBUTORS BE LIABLE 
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR 
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER 
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT 
// LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY 
// WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY 
// OF SUCH DAMAGE.
//
// Author Marc Gamell, Eric Valenzuela, Keita Teranishi, Manish Parashar
//        and Michael Heroux
//
// Questions? Contact Keita Teranishi (knteran@sandia.gov) and
//                    Marc Gamell (mgamell@cac.rutgers.edu)
//
// ************************************************************************
*/

</pre>

## Build Instructions

### Installing ULFM

Please install ULFM from: https://bitbucket.org/icldistcomp/ulfm

## Enabling ULFM

In UTK ULFM implementation (based on OpenMPI), use the following flag to enable ULFM at runtime:

```
mpirun -am ft-enable-mpi
```

In MPICH, you need to pass the following parameter:

```
mpirun -disable-auto-cleanup
```

### Installing Fenix

modify 'run_cmake.sh' 

```

cmake  /path_to_fenix_dir \
 -DCMAKE_C_COMPILER:STRING=/path_to_ulfm_dir/mpi-ulfm/bin/mpicc \
 -DCMAKE_CXX_COMPILER:STRING=/path_to_ulfm_dir/mpi-ulfm/bin/mpic++

```

Then: 

```
chmod +x run_cmake.sh
./run_cmake.sh
```

### Running Fenix Examples

BATCH SYSTEM:

```
#!/bin/bash
#SBATCH --job-name=fenix_example
#SBATCH --output=fenix_example.out
#SBATCH --error=fenix_example.err
#SBATCH --nodes=2                
#SBATCH --time=00:02:00

/path_to_ulfm_dir/bin/mpirun --mca pml ob1 -am ft-enable-mpi --mca btl openib,sm,self --npernode 4 -n 8 ./mysim_fenix_example spare_ranks
```

NON-BATCH SYSTEM:

```
/path_to_fenix_dir/bin/mpirun -np 8 -am ft-enable-mpi ./mysim_fenix_example spare_ranks
```





















