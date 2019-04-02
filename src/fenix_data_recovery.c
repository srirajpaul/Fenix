/*
//@HEADER
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
// THIS SOFTWARE IS PROVIDED BY SANDIA CORPORATION "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL SANDIA CORPORATION OR THE
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Author Marc Gamell, Eric Valenzuela, Keita Teranishi, Manish Parashar
//        and Michael Heroux
//
// Questions? Contact Keita Teranishi (knteran@sandia.gov) and
//                    Marc Gamell (mgamell@cac.rutgers.edu)
//
// ************************************************************************
//@HEADER
*/



#include "fenix_data_recovery.h"
#include "fenix_data_policy.h"
#include "fenix_opt.h"
//#include "fenix_process_recovery.h"
#include "fenix_util.h"
#include "fenix_ext.h"


/**
 * @brief           create new group or recover group data for lost processes
 * @param groud_id  
 * @param comm
 * @param time_start
 * @param depth
 */
int __fenix_group_create( int groupid, MPI_Comm comm, int timestart, int depth, int policy_name, 
        void* policy_value, int* flag) {

  int retval = -1;

  /* Retrieve the array index of the group maintained under the cover. */
  int group_index = __fenix_search_groupid( groupid, fenix.data_recovery );

  if (fenix.options.verbose == 12) {

    verbose_print("c-rank: %d, group_index: %d\n",   __fenix_get_current_rank(*fenix.new_world), group_index);

  }


  /* Check the integrity of user's input */
  if ( timestart < 0 ) {

    debug_print("ERROR Fenix_Data_group_create: time_stamp <%d> must be greater than or equal to zero\n", timestart);
    retval = FENIX_ERROR_INVALID_TIMESTAMP;

  } else if ( depth < -1 ) {

    debug_print("ERROR Fenix_Data_group_create: depth <%d> must be greater than or equal to -1\n",depth);
    retval = FENIX_ERROR_INVALID_DEPTH;

  } else {

    /* This code block checks the need for data recovery.   */
    /* If so, recover the data and set the recovery         */
    /* for member recovery.                                 */

    int i, group_position;
    int remote_need_recovery;
    fenix_group_t *group;
    MPI_Status status;

    fenix_data_recovery_t *data_recovery = fenix.data_recovery;

    /* Initialize Group.  The group hasn't been created.       */
    /* I am either a brand-new process or a recovered process. */
    if (group_index == -1 ) {

      if (fenix.options.verbose == 12 &&   __fenix_get_current_rank(comm) == 0) {
         printf("this is a new group!\n"); 
      }

      data_recovery->count++;
      /* Obtain an available group slot */
      group_index = __fenix_find_next_group_position(data_recovery);
      group = (data_recovery->group[ group_index ] );

      /* Initialize Group */
      __fenix_policy_get_group(data_recovery->group + group_index, comm, timestart, 
              depth, policy_name, policy_value, flag);

      if ( fenix.options.verbose == 12) {
        verbose_print(
                "c-rank: %d, g-groupid: %d, g-timestart: %d, g-depth: %d, g-state: %d\n",
                __fenix_get_current_rank(*fenix.new_world), group->groupid,
                group->timestart,
                group->depth,
                group->state);
      }

    } else { /* Already created. Renew the MPI communicator  */

      group = ( data_recovery->group[group_position] );
      group->comm = comm; /* Renew communicator */

      //Reinit group metadata, communicate with recovered
      //ranks if need be.
      group->vtbl.reinit(group);
    }


    /* Global agreement among the group */
    retval = ( __fenix_join_group(data_recovery, group, comm) != 1) ? FENIX_SUCCESS
                                                    : FENIX_ERROR_GROUP_CREATE;
  }
  return retval;
}

/**
 * @brief
 * @param group_id
 * @param member_id
 * @param data
 * @param count
 * @param data_type
 */
int __fenix_member_create(int groupid, int memberid, void *data, int count, MPI_Datatype datatype ) {

  int retval = -1;
  int group_index = __fenix_search_groupid( groupid, fenix.data_recovery );
  int member_index = __fenix_search_memberid( group_index, memberid );

  if (fenix.options.verbose == 13) {
    verbose_print("c-rank: %d, group_index: %d, member_index: %d\n",
                   __fenix_get_current_rank(*fenix.new_world),
                  group_index, member_index);
  }

  if (group_index == -1) {
    debug_print("ERROR Fenix_Data_member_create: group_id <%d> does not exist\n",
                groupid);
    retval = FENIX_ERROR_INVALID_GROUPID;
  } else if (member_index != -1) {
    debug_print("ERROR Fenix_Data_member_create: member_id <%d> already exists\n",
                memberid);
    retval = FENIX_ERROR_INVALID_MEMBERID;

  } else {

    fenix_group_t *group = (fenix.data_recovery->group[group_index]);
    int retval = group->vtbl.member_create(group, memberid, data, count, datatype);

  }
  return retval;
  /* No Potential Bug in 2/10/17 */
}


/**
 * @brief
 * @param request
 */
int __fenix_data_wait( Fenix_Request request ) {
  int retval = -1;
  int result = __fenix_mpi_wait(&(request.mpi_recv_req));

  if (result != MPI_SUCCESS) {
    retval = FENIX_SUCCESS;
  } else {
    retval = FENIX_ERROR_DATA_WAIT;
  }

  result = __fenix_mpi_wait(&(request.mpi_send_req));

  if (result != MPI_SUCCESS) {
    retval = FENIX_SUCCESS;
  } else {
    retval = FENIX_ERROR_DATA_WAIT;
  }

  return retval;
}

/**
 * @brief
 * @param request
 * @param flag
 */
int __fenix_data_test(Fenix_Request request, int *flag) {
  int retval = -1;
  int result = ( __fenix_mpi_test(&(request.mpi_recv_req)) & __fenix_mpi_test(&(request.mpi_send_req))) ;

  if ( result == 1 ) {
    *flag = 1;
    retval = FENIX_SUCCESS;
  } else {
    *flag = 0 ; // incomplete error?
    retval = FENIX_ERROR_DATA_WAIT;
  }
  return retval;
  /* Good 2/10/17 */
}

/**
 * @brief // TODO: implement FENIX_DATA_MEMBER_ALL
 * @param group_id
 * @param member_id
 * @param subset_specifier
 *
 */

int __fenix_member_store(int groupid, int memberid, Fenix_Data_subset specifier) {
  int retval = -1;
  int group_index = __fenix_search_groupid(groupid, fenix.data_recovery );
  int member_index = -1;

  /* Check if the member id already exists. If so, the index of the storage space is assigned */
  if (memberid != FENIX_DATA_MEMBER_ALL) {
    member_index = __fenix_search_memberid(group_index, memberid);
  }

  if (fenix.options.verbose == 18 && fenix.data_recovery->group[group_index]->current_rank== 0 ) {
    verbose_print(
            "c-rank: %d, role: %d, group_index: %d, member_index: %d memberid: %d\n",
              __fenix_get_current_rank(*fenix.new_world), fenix.role, group_index,
            member_index, memberid);
  }

  if (group_index == -1) {
    debug_print("ERROR Fenix_Data_member_store: group_id <%d> does not exist\n", groupid);
    retval = FENIX_ERROR_INVALID_GROUPID;
  } else if (member_index == -1) {
    debug_print("ERROR Fenix_Data_member_store: member_id <%d> does not exist\n",
                memberid);
    retval = FENIX_ERROR_INVALID_MEMBERID;
  } else {
    fenix_group_t *group = (fenix.data_recovery->group[group_index]);
    retval = group->vtbl.member_store(group, memberid, specifier);
  }
  return retval;
}

/**
 * @brief
 * @param group_id
 * @param member_id
 * @param subset_specifier
 * @param request
 */
int __fenix_member_istore(int groupid, int memberid, Fenix_Data_subset specifier,
                  Fenix_Request *request) {

  int retval = -1;
  int group_index = __fenix_search_groupid(groupid, fenix.data_recovery );
  int member_index = -1;

  /* Check if the member id already exists. If so, the index of the storage space is assigned */
  if (memberid != FENIX_DATA_MEMBER_ALL) {
    member_index = __fenix_search_memberid(group_index, memberid);
  }

  if (fenix.options.verbose == 18 && fenix.data_recovery->group[group_index]->current_rank== 0 ) {
    verbose_print(
            "c-rank: %d, role: %d, group_index: %d, member_index: %d memberid: %d\n",
              __fenix_get_current_rank(*fenix.new_world), fenix.role, group_index,
            member_index, memberid);
  }

  if (group_index == -1) {
    debug_print("ERROR Fenix_Data_member_store: group_id <%d> does not exist\n", groupid);
    retval = FENIX_ERROR_INVALID_GROUPID;
  } else if (member_index == -1) {
    debug_print("ERROR Fenix_Data_member_store: member_id <%d> does not exist\n",
                memberid);
    retval = FENIX_ERROR_INVALID_MEMBERID;
  } else {
    fenix_group_t *group = (fenix.data_recovery->group[group_index]);
    retval = group->vtbl.member_istore(group, memberid, specifier, request);
  }
  return retval;
}



void __fenix_subset(fenix_group_t *group, fenix_member_entry_t *me, Fenix_Data_subset *ss) {
#if 1
  fprintf(stderr,"ERROR Fenix_Subset is not currently supported\n");

#else
  fenix_version_t *version = &(me->version);
  fenix_local_entry_t *lentry = &(version->local_entry[version->position]);
  fenix_remote_entry_t *rentry = &(version->remote_entry[version->position]);

  int i; 
  MPI_Status status;
  
  /* Store the local data */
  /* This version does not apply any storage saving scheme */
  memcpy(lentry->data, lentry->pdata, (lentry->count * lentry->size));

  /* Check the subset */
  int subset_total_size = 0;
  for( i = 0; i < ss->num_blocks; i++ ) {
    int subset_start = ss->start_offset[i];
    int subset_end = ss->start_offset[i];
    int subset_stride = ss->start_offset[i];
   
  }
  subset_total_size = ss->num_blocks * ss->fblk_size;

  /* Create a buffer for sending data  (lentry->size is a size of single element ) */
  void *subset_data = (void *) s_malloc(me->datatype_size *  subset_total_size );
   

  /* This data exchange is not necessary when using non-v call */ 
  member_store_packet_t lentry_packet, rentry_packet;
  lentry_packet.rank = lentry->currentrank;
  lentry_packet.datatype = lentry->datatype;
  lentry_packet.entry_count = lentry->count;
  lentry_packet.entry_size = subset_total_size;

  int current_rank =   __fenix_get_current_rank(*fenix.new_world);
  int current_role = fenix.role;

  MPI_Sendrecv(&lentry_packet, sizeof(member_store_packet_t), MPI_BYTE, ge->out_rank,
                 STORE_SIZE_TAG, &rentry_packet, sizeof(member_store_packet_t), MPI_BYTE,
                 ge->in_rank, STORE_SIZE_TAG, (ge->comm), &status);
  
  rentry->remoterank = rentry_packet.rank;
  rentry->datatype = rentry_packet.datatype;
  rentry->count = rentry_packet.entry_count;
  rentry->size = rentry_packet.entry_size;

  if (rentry->data != NULL) {
      rentry->data = s_malloc(rentry->count * rentry->size);
  }

  /* Partner is sending subset */
  if( rentry->size != rentry->count  ) {
    /* Receive # of blocks */

  } 
  /* Handle Subset */
  int subset_num_blocks = ss->num_blocks;
  int subset_start = ss->start_offsets[0];
  int subset_end = ss->end_offsets[0];
  int subset_stride = ss->stride;
  int subset_diff = subset_end - subset_start;
  int subset_count = subset_num_blocks * subset_diff;

  int subset_block = 0;
  int subset_index = 0;
  void *subset_data = (void *) s_malloc(sizeof(void) * me->current_count);

  int data_index;
  int data_steps = 0;
  int data_count = me->current_count;
  for (data_index = subset_start; (subset_block != subset_num_blocks - 1) &&
                       (data_index < data_count); data_index++) {
    if (data_steps != subset_diff) {
      MPI_Sendrecv((lentry->data) + data_index, (1 * lentry->size), MPI_BYTE, ge->out_rank,
                   STORE_DATA_TAG, (rentry->data) + data_index, (1 * rentry->size), MPI_BYTE,
                   ge->in_rank, STORE_DATA_TAG, ge->comm, &status);
      // memcpy((subset_data) + data_index, (me->current_buf) + data_index, sizeof(me->current_datatype));
      data_steps = data_steps + 1;
    } else if (data_steps == subset_diff) {
      data_steps = 0;
      subset_block = subset_block + 1;
      data_index = data_index + subset_stride - 1;
    }
  } 

  /* Need to update the version info */
  if (version->position < version->size - 1) {
      version->num_copies++;
      version->position++;
    } else { /* Back to 0 */
        version->position = 0;
    }
#endif
}

fenix_local_entry_t *__fenix_subset_variable(fenix_member_entry_t *me, Fenix_Data_subset *ss) {
#if 1
  fprintf(stderr,"ERROR Fenix_Subset is not currently supported\n");

#else
  int ss_num_blocks = ss->num_blocks;
  int *ss_start = (int *) s_malloc(ss_num_blocks * sizeof(int));
  int *ss_end = (int *) s_malloc(ss_num_blocks * sizeof(int));;
  memcpy(ss_start, ss->start_offsets, (ss_num_blocks * sizeof(int)));
  memcpy(ss_end, ss->end_offsets, (ss_num_blocks * sizeof(int)));

  int index;
  int ss_count = 0;
  for (index = 0; index < ss_num_blocks; index++) {
    ss_count = ss_count + (ss_end[index] - ss_start[index]);
  }

  int offset_index = 0;
  int ss_block = 0;
  int ss_index = 0;
  int ss_diff = ss_end[0] - ss_start[0]; // init diff
  void *ss_data = (void *) s_malloc(sizeof(void) * ss_count);

  int data_index;
  int data_steps = 0;
  int data_count = me->current_count;
  for (data_index = 0;
       (ss_block != ss_num_blocks) && (data_index < data_count); data_index++) {
    if (data_index >= ss_start[offset_index] && data_index <= ss_end[offset_index]) {
      ss_diff = ss_end[offset_index] - ss_start[offset_index];
      memcpy((ss_data) + ss_index, (me->user_data) + data_index,
             sizeof(me->current_datatype));
      ss_index = ss_index + 1;
      data_steps = data_steps + 1;
    }
    if (data_steps == ss_diff) {
      data_steps = 0;
      offset_index = offset_index + 1;
      ss_block = ss_block + 1;
    }
  }

  fenix_local_entry_t *lentry = (fenix_local_entry_t *) s_malloc(
          sizeof(fenix_local_entry_t));
  lentry->currentrank = me->currentrank;
  lentry->count = ss_count;
  lentry->datatype = me->current_datatype;
  lentry->pdata = ss_data;
  lentry->size = sizeof(ss_data);
  lentry->data = s_malloc(lentry->count * lentry->size);
  memcpy(lentry->data, lentry->pdata, (lentry->count * lentry->size));

  return lentry;
  #endif


  return NULL;
}


#if 0
/**
 * @brief
 * @param group_id
 * @param member_id
 * @param subset_specifier
 */
int __fenix_member_storev(int group_id, int member_id, Fenix_Data_subset subset_specifier) {

/*
 * Using the same routine for v and non-v routine.
 */
  int retval = -1;
  int group_index = __fenix_search_groupid( group_id, fenix.data_recovery );
  int member_index = __fenix_search_memberid(group_index, member_id);
  if (group_index == -1) {
    debug_print("ERROR Fenix_Data_member_storev: group_id <%d> does not exist\n",
                group_id);
    retval = FENIX_ERROR_INVALID_GROUPID;
  } else if (member_index == -1) {
    debug_print("ERROR Fenix_Data_member_storev: member_id <%d> does not exist\n",
                member_id);
    retval = FENIX_ERROR_INVALID_MEMBERID;
  } else {
    fenix_group_t *group = fenix.data_recovery;
    fenix_group_entry_t *gentry = &(group->group_entry[group_index]);
    fenix_member_t *member = &(gentry->member);
    __fenix_ensure_version_capacity(member);
    fenix_member_entry_t *mentry = &(member->member_entry[member_index]);
    fenix_version_t *version = &(mentry->version);
    fenix_local_entry_t *lentry = &(version->local_entry[version->position]);
    fenix_remote_entry_t *rentry = &(version->remote_entry[version->position]);
    retval = FENIX_SUCCESS;
  }
  return retval;

}
#endif

#if 0
/**
 * @brief
 * @param group_id
 * @param member_id
 * @param subset_specifier
 * @param request 
 */
int __fenix_member_istorev(int group_id, int member_id, Fenix_Data_subset subset_specifier,
                   Fenix_Request *request) {

  int retval = -1;
  int group_index = __fenix_search_groupid(group_id, __fenixi_g_data_recovery );
  int member_index = __fenix_search_memberid(group_index, member_id);
  if (group_index == -1) {
    debug_print("ERROR Fenix_Data_member_istorev: group_id <%d> does not exist\n",
                group_id);
    retval = FENIX_ERROR_INVALID_GROUPID;
  } else if (member_index == -1) {
    debug_print("ERROR Fenix_Data_member_istorev: member_id <%d> does not exist\n",
                member_id);
    retval = FENIX_ERROR_INVALID_MEMBERID;
  } else {
    fenix_group_t *group = fenix.data_recovery;
    fenix_group_entry_t *gentry = &(group->group_entry[group_index]);
    fenix_member_t *member = &(gentry->member);
    __fenix_ensure_version_capacity(member);
    fenix_member_entry_t *mentry = &(member->member_entry[member_index]);
    fenix_version_t *version = &(mentry->version);
    fenix_local_entry_t *lentry = &(version->local_entry[version->position]);
    fenix_remote_entry_t *rentry = &(version->remote_entry[version->position]);
    retval = FENIX_SUCCESS;
  }

  return retval;
}
#endif

/**
 * @brief
 * @param group_id
 * @param time_stamp
 */
int __fenix_data_commit(int groupid, int *timestamp) {
  /* No communication is performed */
  /* Return the new timestamp      */
  int retval = -1;
  int group_index = __fenix_search_groupid(groupid, fenix.data_recovery );
  if (fenix.options.verbose == 22) {
    verbose_print("c-rank: %d, role: %d, group_index: %d\n",   __fenix_get_current_rank(*fenix.new_world), fenix.role, group_index);
  }
  if (group_index == -1) {
    debug_print("ERROR Fenix_Data_commit: group_id <%d> does not exist\n", groupid);
    retval = FENIX_ERROR_INVALID_GROUPID;
  } else {
    fenix_group_t *group = (fenix.data_recovery->group[group_index]);
    
    group->vtbl.commit(group); 
    
    if (timestamp != NULL) {
      *timestamp = group->timestamp;
    }

    retval = FENIX_SUCCESS;
  }
  return retval;
}

/**
 * @brief
 * @param group_id
 * @param time_stamp
 */
int __fenix_data_commit_barrier(int groupid, int *timestamp) {
  int retval = -1;
  int group_index = __fenix_search_groupid(groupid, fenix.data_recovery );
  if (fenix.options.verbose == 23) {
    verbose_print("c-rank: %d, role: %d, group_index: %d\n",
                    __fenix_get_current_rank(*fenix.new_world), fenix.role, group_index);
  }
  if (group_index == -1) {
    debug_print("ERROR Fenix_Data_commit: group_id <%d> does not exist\n", groupid);
    retval = FENIX_ERROR_INVALID_GROUPID;
  } else {
    fenix_group_t *group = (fenix.data_recovery->group[group_index]);
    int min_timestamp;
    MPI_Allreduce( &(group->timestamp), &min_timestamp, 1, MPI_INT, MPI_MIN,  group->comm );

    retval = group->vtbl.commit(group);
    
    if (timestamp != NULL) {
      *timestamp = group->timestamp;
    }
  }
  return retval;
}


/**
 * @brief
 * @param group_id
 * @param member_id
 * @param data
 * @param max_count
 * @param time_stamp
 */
int __fenix_member_restore(int groupid, int memberid, void *data, int maxcount, int timestamp) {

  int retval =  FENIX_SUCCESS;
  int group_index = __fenix_search_groupid(groupid, fenix.data_recovery);
  int member_index = __fenix_search_memberid(group_index, memberid);

  if (fenix.options.verbose == 25) {
    verbose_print("c-rank: %d, role: %d, group_index: %d, member_index: %d\n",
                    __fenix_get_current_rank(*fenix.new_world), fenix.role, group_index,
                  member_index);
  }

  if (group_index == -1) {
    debug_print("ERROR Fenix_Data_member_restore: group_id <%d> does not exist\n",
                groupid);
    retval = FENIX_ERROR_INVALID_GROUPID;
  } else {
    fenix_group_t *group = (fenix.data_recovery->group[group_index]);
    retval = group->vtbl.member_restore(group, memberid, data, maxcount, timestamp);
  }
  return retval;
}

/**
 * @brief
 * @param group_id
 * @param member_id
 * @param target_buffer
 * @param max_count
 * @param time_stamp
 * @param source_rank
 */
int __fenix_member_restore_from_rank(int groupid, int memberid, void *target_buffer,
                             int max_count, int time_stamp, int source_rank) {
  int retval =  FENIX_SUCCESS;
  int group_index = __fenix_search_groupid(groupid, fenix.data_recovery);
  int member_index = __fenix_search_memberid(group_index, memberid);

  if (fenix.options.verbose == 25) {
    verbose_print("c-rank: %d, role: %d, group_index: %d, member_index: %d\n",
                    __fenix_get_current_rank(*fenix.new_world), fenix.role, group_index,
                  member_index);
  }

  if (group_index == -1) {
    debug_print("ERROR Fenix_Data_member_restore: group_id <%d> does not exist\n",
                groupid);
    retval = FENIX_ERROR_INVALID_GROUPID;
  } else {
    fenix_group_t *group = (fenix.data_recovery->group[group_index]);
    retval = group->vtbl.member_restore_from_rank(group, memberid, target_buffer, 
            max_count, time_stamp, source_rank);
  }
  return retval;
}

/**
 * @brief
 * @param num_blocks
 * @param start_offset
 * @param end_offset
 * @param stride
 * @param subset_specifier
 *
 * This routine creates 
 */
int __fenix_data_subset_create(int num_blocks, int start_offset, int end_offset, int stride,
                       Fenix_Data_subset *subset_specifier) {
  int retval = -1;
  if (num_blocks <= 0) {
    debug_print("ERROR Fenix_Data_subset_create: num_blocks <%d> must be positive\n",
                num_blocks);
    retval = FENIX_ERROR_SUBSET_NUM_BLOCKS;
  } else if (start_offset < 0) {
    debug_print("ERROR Fenix_Data_subset_create: start_offset <%d> must be positive\n",
                start_offset);
    retval = FENIX_ERROR_SUBSET_START_OFFSET;
  } else if (end_offset <= 0) {
    debug_print("ERROR Fenix_Data_subset_create: end_offset <%d> must be positive\n",
                end_offset);
    retval = FENIX_ERROR_SUBSET_END_OFFSET;
  } else if (stride <= 0) {
    debug_print("ERROR Fenix_Data_subset_create: stride <%d> must be positive\n", stride);
    retval = FENIX_ERROR_SUBSET_STRIDE;
  } else {
    subset_specifier->start_offsets = (int *) s_malloc(sizeof(int));
    subset_specifier->end_offsets = (int *) s_malloc(sizeof(int));
    subset_specifier->num_blocks = num_blocks;
    subset_specifier->start_offsets[0] = start_offset;
    subset_specifier->end_offsets[0] = end_offset;
    subset_specifier->stride = stride;
    subset_specifier->specifier = __FENIX_SUBSET_CREATE;
    retval = FENIX_SUCCESS;
  }
  return retval;
}

/**
 * @brief
 * @param num_blocks
 * @param array_start_offsets
 * @param array_end_offsets
 * @param subset_specifier
 */
int __fenix_data_subset_createv(int num_blocks, int *array_start_offsets, int *array_end_offsets,
                        Fenix_Data_subset *subset_specifier) {

  int retval = -1;
  if (num_blocks <= 0) {
    debug_print("ERROR Fenix_Data_subset_createv: num_blocks <%d> must be positive\n",
                num_blocks);
    retval = FENIX_ERROR_SUBSET_NUM_BLOCKS;
  } else if (array_start_offsets == NULL) {
    debug_print( "ERROR Fenix_Data_subset_createv: array_start_offsets %s must be at least of size 1\n", "");
    retval = FENIX_ERROR_SUBSET_START_OFFSET;
  } else if (array_end_offsets == NULL) {
    debug_print( "ERROR Fenix_Data_subset_createv: array_end_offsets %s must at least of size 1\n", "");
    retval = FENIX_ERROR_SUBSET_END_OFFSET;
  } else {

    // first check that the start offsets and end offsets are valid
    int index;
    int invalid_index = -1;
    int found_invalid_index = 0;
    for (index = 0; found_invalid_index != 1 && (index < num_blocks); index++) {
      if (array_start_offsets[index] > array_end_offsets[index]) {
        invalid_index = index;
        found_invalid_index = 1;
      }
    }

    if (found_invalid_index != 1) { // if not true (!= 1)
      subset_specifier->num_blocks = num_blocks;

      subset_specifier->start_offsets = (int *)s_malloc(sizeof(int)* num_blocks);
      memcpy(subset_specifier->start_offsets, array_start_offsets, ( num_blocks * sizeof(int))); // deep copy

      subset_specifier->end_offsets = (int *)s_malloc(sizeof(int)* num_blocks);
      memcpy(subset_specifier->end_offsets, array_end_offsets, ( num_blocks * sizeof(int))); // deep copy

      subset_specifier->specifier = __FENIX_SUBSET_CREATEV; // 
      retval = FENIX_SUCCESS;
    } else {
      debug_print(
              "ERROR Fenix_Data_subset_createv: array_end_offsets[%d] must be less than array_start_offsets[%d]\n",
              invalid_index, invalid_index);
      retval = FENIX_ERROR_SUBSET_END_OFFSET;
    }
  }
  return retval;
}

int __fenix_data_subset_free( Fenix_Data_subset *subset_specifier ) {
  int  retval = FENIX_SUCCESS;;
  free( subset_specifier->start_offsets );
  free( subset_specifier->end_offsets );
  subset_specifier->specifier = __FENIX_SUBSET_UNDEFINED;
  return retval;
}

/**
 * @brief
 * @param subset_specifier
 */
int __fenix_data_subset_delete( Fenix_Data_subset *subset_specifier ) {
  __fenix_data_subset_free(subset_specifier);
  free(subset_specifier);
  return FENIX_SUCCESS;
}

/**
 * @brief
 * @param group_id
 * @param num_members
 */
int __fenix_get_number_of_members(int group_id, int *num_members) {
  int retval = -1;
  int group_index = __fenix_search_groupid(group_id, fenix.data_recovery );
  if (group_index == -1) {
    debug_print("ERROR Fenix_Data_commit: group_id <%d> does not exist\n", group_id);
    retval = FENIX_ERROR_INVALID_GROUPID;
  } else {
    fenix_group_t *group = (fenix.data_recovery->group[group_index]);
    *num_members = group->member->count;
    retval = FENIX_SUCCESS;
  }
  return retval;
}

/**
 * @brief
 * @param group_id
 * @param member_id
 * @param position
 */
int __fenix_get_member_at_position(int group_id, int *member_id, int position) {
  int retval = -1;
  int group_index = __fenix_search_groupid(group_id, fenix.data_recovery);
  if (group_index == -1) {
    debug_print("ERROR Fenix_Data_commit: group_id <%d> does not exist\n", group_id);
    retval = FENIX_ERROR_INVALID_GROUPID;
  } else {
    fenix_group_t *group = (fenix.data_recovery->group[group_index]);
    fenix_member_t *member = group->member;
    if (position < 0 || position > (member->total_size) - 1) {
      debug_print(
              "ERROR Fenix_Data_group_get_member_at_position: position <%d> must be a value between 0 and number_of_members-1 \n",
              position);
      retval = FENIX_ERROR_INVALID_POSITION;
    } else {
      int member_index = ((member->total_size) - 1) - position;
      fenix_member_entry_t *mentry = &(member->member_entry[member_index]);
      *member_id = mentry->memberid;
      retval = FENIX_SUCCESS;
    }
  }
  return retval;
}

/**
 * @brief
 * @param group_id
 * @param num_snapshots
 */
int __fenix_get_number_of_snapshots(int group_id, int *num_snapshots) {
  int retval = -1;
  int group_index = __fenix_search_groupid(group_id, fenix.data_recovery );
  if (group_index == -1) {
    debug_print("ERROR Fenix_Data_commit: group_id <%d> does not exist\n", group_id);
    retval = FENIX_ERROR_INVALID_GROUPID;
  } else {
    fenix_group_t *group = (fenix.data_recovery->group[group_index]);
    fenix_member_t *member = group->member;
    fenix_member_entry_t *mentry = &(member->member_entry[0]); // does not matter which member you get, every member has the same number of versions 
    fenix_version_t *version = mentry->version;
    *num_snapshots = version->count;
    retval = FENIX_SUCCESS;
  }
  return retval;
}

/**
 * @brief
 * @param group_id
 * @param position
 * @param time_stamp
 */
int __fenix_get_snapshot_at_position(int groupid, int position, int *timestamp) {
  int retval = -1;
  int group_index = __fenix_search_groupid(groupid, fenix.data_recovery );
  if (fenix.options.verbose == 33) {
    verbose_print("c-rank: %d, role: %d, group_index: %d\n",
                    __fenix_get_current_rank(*fenix.new_world), fenix.role, group_index);
  }
  if (group_index == -1) {
    debug_print("ERROR Fenix_Data_commit: group_id <%d> does not exist\n", groupid);
    retval = FENIX_ERROR_INVALID_GROUPID;
  } else {
    fenix_group_t *group = (fenix.data_recovery->group[group_index]);
    *timestamp = group->timestamp - position;
  }
  return retval;
}

/**
 * @brief
 * @param group_id
 * @param member_id
 * @param attribute_name
 * @param attribute_value
 * @param flag
 * @param source_rank
 */
int __fenix_member_get_attribute(int groupid, int memberid, int attributename,
                         void *attributevalue, int *flag, int sourcerank) {
  int retval = -1;
  int group_index = __fenix_search_groupid(groupid, fenix.data_recovery );
  int member_index = __fenix_search_memberid(group_index, memberid);
  if (fenix.options.verbose == 34) {
    verbose_print("c-rank: %d, role: %d, group_index: %d, member_index: %d\n",
                    __fenix_get_current_rank(*fenix.new_world), fenix.role, group_index,
                  member_index);
  }
  if (group_index == -1) {
    debug_print("ERROR Fenix_Data_member_attr_get: group_id <%d> does not exist\n",
                groupid);
    retval = FENIX_ERROR_INVALID_GROUPID;
  } else if (member_index == -1) {
    debug_print("ERROR Fenix_Data_member_attr_get: member_id <%d> does not exist\n",
                memberid);
    retval = FENIX_ERROR_INVALID_MEMBERID;
  } else {
    fenix_group_t *group = (fenix.data_recovery->group[group_index]);
    fenix_member_t *member = group->member;
    __fenix_ensure_version_capacity_from_member(member);
    fenix_member_entry_t *mentry = &(member->member_entry[member_index]);
    fenix_version_t *version = mentry->version;
    fenix_buffer_entry_t *lentry = &(version->local_entry[version->position]);

    switch (attributename) {
      case FENIX_DATA_MEMBER_ATTRIBUTE_BUFFER:
        attributevalue = mentry->user_data;
        retval = FENIX_SUCCESS;
        break;
      case FENIX_DATA_MEMBER_ATTRIBUTE_COUNT:
        *((int *) (attributevalue)) = mentry->current_count;
        retval = FENIX_SUCCESS;
        break;
      case FENIX_DATA_MEMBER_ATTRIBUTE_DATATYPE:
        *((MPI_Datatype *)attributevalue) = mentry->current_datatype;
        retval = FENIX_SUCCESS;
        break;
#if 0
      case FENIX_DATA_MEMBER_ATTRIBUTE_SIZE:
        *((int *) (attributevalue)) = lentry->size;
        retval = FENIX_SUCCESS;
        break;
#endif
      default:
        debug_print("ERROR Fenix_Data_member_attr_get: invalid attribute_name <%d>\n",
                    attributename);
        retval = FENIX_ERROR_INVALID_ATTRIBUTE_NAME;
        break;
    }

  }
  return retval;
}

/**
 * @brief
 * @param group_id
 * @param member_id
 * @param attribute_name
 * @param attribute_value
 * @param flag
 */
int __fenix_member_set_attribute(int groupid, int memberid, int attributename,
                         void *attributevalue, int *flag) {
  int retval = -1;
  int group_index = __fenix_search_groupid(groupid, fenix.data_recovery );
  int member_index = __fenix_search_memberid(group_index, memberid);
  if (fenix.options.verbose == 35) {
    verbose_print("c-rank: %d, role: %d, group_index: %d, member_index: %d\n",
                    __fenix_get_current_rank(*fenix.new_world), fenix.role, group_index,
                  member_index);
  }
  if (group_index == -1) {
    debug_print("ERROR Fenix_Data_member_attr_set: group_id <%d> does not exist\n",
                groupid);
    retval = FENIX_ERROR_INVALID_GROUPID;
  } else if (member_index == -1) {
    debug_print("ERROR Fenix_Data_member_attr_set: member_id <%d> does not exist\n",
                memberid);
    retval = FENIX_ERROR_INVALID_MEMBERID;
  } else if (fenix.role == 0) {
    debug_print("ERROR Fenix_Data_member_attr_set: cannot be called on role: <%s> \n",
                "FENIX_ROLE_INITIAL_RANK");
    retval = FENIX_ERROR_INVALID_LOGIC_CALL;
  } else {
    int my_datatype_size;
    int myerr;
    fenix_group_t *group = (fenix.data_recovery->group[group_index]);
    fenix_member_t *member = group->member;
    __fenix_ensure_version_capacity_from_member(member);
    fenix_member_entry_t *mentry = &(member->member_entry[member_index]);
    fenix_version_t *version = mentry->version;
    fenix_buffer_entry_t *lentry = &(version->local_entry[version->position]);

    switch (attributename) {
      case FENIX_DATA_MEMBER_ATTRIBUTE_BUFFER:
        mentry->user_data = attributevalue;
        break;
      case FENIX_DATA_MEMBER_ATTRIBUTE_COUNT:
        mentry->current_count = *((int *) (attributevalue));
        lentry->count = *((int *) (attributevalue));
        retval = FENIX_SUCCESS;
        break;
      case FENIX_DATA_MEMBER_ATTRIBUTE_DATATYPE:

        myerr = MPI_Type_size(*((MPI_Datatype *)(attributevalue)), &my_datatype_size);

        if( myerr ) {
          debug_print(
                  "ERROR Fenix_Data_member_attr_get: Fenix currently does not support this MPI_DATATYPE; invalid attribute_value <%p>\n",
                  attributevalue);
          retval = FENIX_ERROR_INVALID_ATTRIBUTE_NAME;
        }

        mentry->current_datatype = *((MPI_Datatype *)(attributevalue));
        lentry->datatype = *((MPI_Datatype *)(attributevalue));
        mentry->datatype_size = my_datatype_size;
        lentry->datatype_size = my_datatype_size;
        retval = FENIX_SUCCESS;
        break;
#if 0
        case FENIX_DATA_MEMBER_ATTRIBUTE_SIZE:
        lentry->size = *((int *) (attributevalue));
        retval = FENIX_SUCCESS;
        break;
#endif
      default:
        debug_print("ERROR Fenix_Data_member_attr_get: invalid attribute_name <%d>\n",
                    attributename);
        retval = FENIX_ERROR_INVALID_ATTRIBUTE_NAME;
        break;
    }

    retval = FENIX_SUCCESS;
  }
  return retval;
}

/**
 * @brief
 * @param group_id
 * @param time_stamp
 */
int __fenix_snapshot_delete(int group_id, int time_stamp) {
  int retval = -1;
  int group_index = __fenix_search_groupid(group_id, fenix.data_recovery );
  if (group_index == -1) {
    debug_print("ERROR Fenix_Data_snapshot_delete: group_id <%d> does not exist\n",
                group_id);
    retval = FENIX_ERROR_INVALID_GROUPID;
  } else if (time_stamp < 0) {
    debug_print(
            "ERROR Fenix_Data_snapshot_delete: time_stamp <%d> must be greater than zero\n",
            time_stamp);
    retval = FENIX_ERROR_INVALID_TIMESTAMP;
  } else {
    fenix_group_t *group = (fenix.data_recovery->group[group_index]);
    retval = group->vtbl.snapshot_delete(group, time_stamp);
  }
  return retval;
}


/**
 * @brief
 * @param group_id
 */
int __fenix_group_delete(int groupid) {
  /******************************************/
  /* Commit needs to confirm the completion */
  /* of delete. Otherwise, it is reverted.  */
  /******************************************/
  int retval = -1;
  int group_index = __fenix_search_groupid(groupid, fenix.data_recovery );

  if (fenix.options.verbose == 37) {
    verbose_print("c-rank: %d, group_index: %d\n",
                    __fenix_get_current_rank(*fenix.new_world), group_index);
  }

  if (group_index == -1) {
    debug_print("ERROR Fenix_Data_group_delete: group_id <%d> does not exist\n", groupid);
    retval = FENIX_ERROR_INVALID_GROUPID;
  } else {
    /* Delete Process */
    fenix_data_recovery_t *data_recovery = fenix.data_recovery;
    fenix_group_t *group = (data_recovery->group[group_index]);
    retval = group->vtbl.group_delete(group);
    
    if(retval == FENIX_SUCCESS) data_recovery->count--;

  }
  return retval;
}

/**
 * @brief
 * @param group_id
 * @param member_id
 */
int __fenix_member_delete(int groupid, int memberid) {
  int retval = -1;
  int group_index = __fenix_search_groupid(groupid, fenix.data_recovery );
  int member_index = __fenix_search_memberid(group_index, memberid);

  if (fenix.options.verbose == 38) {
    verbose_print("c-rank: %d, role: %d, group_index: %d, member_index: %d\n",
                    __fenix_get_current_rank(*fenix.new_world), fenix.role, group_index,
                  member_index);
  }

  if (group_index == -1) {
    debug_print("ERROR Fenix_Data_member_delete: group_id <%d> does not exist\n",
                groupid);
    retval = FENIX_ERROR_INVALID_GROUPID;
  } else if (member_index == -1) {
    debug_print("ERROR Fenix_Data_member_delete: memberid <%d> does not exist\n",
                memberid);
    retval = FENIX_ERROR_INVALID_MEMBERID;
  } else {
    fenix_data_recovery_t *data_recovery = fenix.data_recovery;
    fenix_group_t *group = (data_recovery->group[group_index]);
    
    retval = group->vtbl.member_delete(group, memberid);
    
    if(retval == FENIX_SUCCESS){
      fenix_member_t *member = group->member;
      member->count--;
      fenix_member_entry_t *mentry = &(member->member_entry[member_index]);
      mentry->state = DELETED;
      fenix_version_t *version = mentry->version;
      version->count = 0;
    }

    if (fenix.options.verbose == 38) {
      fenix_member_t *member = group->member;
      fenix_member_entry_t *mentry = &(member->member_entry[member_index]);
      fenix_version_t *version = mentry->version;
      
      verbose_print("c-rank: %d, role: %d, m-count: %zu, m-state: %d, v-count: %zu\n",
                      __fenix_get_current_rank(*fenix.new_world), fenix.role,
                    member->count, mentry->state,
                    version->count);
    }

    retval = FENIX_SUCCESS;
  }
  return retval;
}


/**
 * @brief
 * @param
 * @param
 */
int __fenix_search_memberid(int group_index, int key) {
  fenix_data_recovery_t *data_recovery = fenix.data_recovery;
  fenix_group_t *group = (data_recovery->group[group_index]);
  fenix_member_t *member = group->member;
  int member_index, found = -1, index = -1;
  for (member_index = 0;
       (found != 1) && (member_index < member->total_size); member_index++) {
    fenix_member_entry_t *mentry = &(member->member_entry[member_index]);
    if (key == mentry->memberid) {
      index = member_index;
      found = 1;
    }
  }
  return index;
}



/**
 * @brief
 * @param
 */
int __fenix_find_next_member_position(fenix_member_t *m) {
  fenix_member_t *member = m;
  int member_index, found = -1, index = -1;
  for (member_index = 0;
       (found != 1) && (member_index < member->total_size); member_index++) {
    fenix_member_entry_t *mentry = &(member->member_entry[member_index]);
    if (mentry->state == EMPTY || mentry->state == DELETED) {
      index = member_index;
      found = 1;
    }
  }
  return index;
}

/**
 * @brief
 * @param
 * @param
 */
int __fenix_join_group(fenix_data_recovery_t *data_recovery, fenix_group_t *group, MPI_Comm comm) {
  int current_rank_attributes[__GROUP_ENTRY_ATTR_SIZE];
  int other_rank_attributes[__GROUP_ENTRY_ATTR_SIZE];
  current_rank_attributes[0] = data_recovery->count;
  current_rank_attributes[1] = group->groupid;
  current_rank_attributes[2] = group->timestamp;
  current_rank_attributes[3] = group->depth;
  current_rank_attributes[4] = group->state;

  if (fenix.options.verbose == 58) {
    verbose_print(
            "c-rank: %d, g-count: %zu, g-groupid: %d, g-timestamp: %d, g-depth: %d, g-state: %d\n",
              __fenix_get_current_rank(*fenix.new_world), data_recovery->count, group->groupid,
            group->timestamp, group->depth, group->state);
  }

  MPI_Allreduce(current_rank_attributes, other_rank_attributes, __GROUP_ENTRY_ATTR_SIZE,
                MPI_INT, fenix.agree_op, comm);

  int found = -1, index;
  for (index = 0; found != 1 && (index < __GROUP_ENTRY_ATTR_SIZE); index++) {
    if (current_rank_attributes[index] !=
        other_rank_attributes[index]) { // all ranks did not agree! 
      switch (index) {
        case 0:
          debug_print("ERROR ranks did not agree on g-count: %d\n",
                      current_rank_attributes[0]);
          break;
        case 1:
          debug_print("ERROR ranks did not agree on g-groupid: %d\n",
                      current_rank_attributes[1]);
          break;
        case 2:
          debug_print("ERROR ranks did not agree on g-timestamp: %d\n",
                      current_rank_attributes[2]);
          break;
        case 3:
          debug_print("ERROR ranks did not agree on g-depth: %d\n",
                      current_rank_attributes[3]);
          break;
        case 4:
          debug_print("ERROR ranks did not agree on g-state: %d\n",
                      current_rank_attributes[4]);
          break;
        default:
          break;
      }
      found = 1;
    }
  }

  return found;
}

/**
 * @brief
 * @param
 * @param
 */
int __fenix_join_member(fenix_member_t *m, fenix_member_entry_t *me, MPI_Comm comm) {
  fenix_member_t *member = m;
  fenix_member_entry_t *mentry = me;
  int current_rank_attributes[__NUM_MEMBER_ATTR_SIZE];
  current_rank_attributes[0] = member->count;
  current_rank_attributes[1] = mentry->memberid;
  current_rank_attributes[2] = mentry->state;
  int other_rank_attributes[__NUM_MEMBER_ATTR_SIZE];
  MPI_Allreduce(current_rank_attributes, other_rank_attributes, __NUM_MEMBER_ATTR_SIZE,
                MPI_INT, fenix.agree_op, comm);
  int found = -1, index;
  for (index = 0; found != 1 && (index < __NUM_MEMBER_ATTR_SIZE); index++) {
    if (current_rank_attributes[index] != other_rank_attributes[index]) {
      switch (index) {
        case 0:
          debug_print("ERROR ranks did not agree on m-count: %d\n",
                      current_rank_attributes[0]);
          break;
        case 1:
          debug_print("ERROR ranks did not agree on m-memberid: %d\n",
                      current_rank_attributes[1]);
          break;
        case 2:
          debug_print("ERROR ranks did not agree on m-state: %d\n",
                      current_rank_attributes[2]);
          break;
        default:
          break;
      }
      found = 1;
    }
  }
  return found;
}

/**
 * @brief
 * @param
 * @param
 */
int __fenix_join_restore(fenix_group_t *group, fenix_version_t *v, MPI_Comm comm) {
  int current_rank_attributes[2];
  int other_rank_attributes[2];
  int found = -1;

/* Find the minimum timestamp among the ranks */
  int min_timestamp, idiff;
  MPI_Allreduce(&(group->timestamp), &min_timestamp, 1, MPI_INT, MPI_MIN, comm);

  idiff = group->timestamp - min_timestamp;
  if ((min_timestamp > (group->timestamp - group->depth))
      && (idiff < v->num_copies)) {
    /* Shift the position of the latest version */
    v->position = (v->position + v->total_size - idiff) % (v->total_size);
    found = 1;
  } else {
    found = -1;
  }

  /* Check if every rank finds the copy */
  int result;
  MPI_Allreduce(&found, &result, 1, MPI_INT, MPI_MIN, comm);

  found = result;
  return found;
}

/**
 * @brief
 * @param
 * @param
 */
int __fenix_join_commit( fenix_group_t *group, fenix_version_t *v, MPI_Comm comm) {
  int found = -1;
  int min_timestamp;
  MPI_Allreduce(&(group->timestamp), &min_timestamp, 1, MPI_INT, MPI_MIN, comm);

  int timestamp_offset = group->timestamp - min_timestamp;
  int depth_offest = (group->timestamp - group->depth);
  if ((min_timestamp > depth_offest) && (timestamp_offset < v->num_copies)) {
    v->position = (v->position + (v->total_size - timestamp_offset)) % (v->total_size);
    found = 1;
  } else {
    found = -1;
  }

  int result;
  MPI_Allreduce(&found, &result, 1, MPI_INT, MPI_MIN, comm);
  found = result;
  return found;
}

///////////////////////////////////////////////////// TODO //

void __fenix_store_single() {


}

/**
 *
 */
void __feninx_dr_print_store() {
  int group, member, version, local, remote;
  fenix_data_recovery_t *current = fenix.data_recovery;
  int group_count = current->count;
  for (group = 0; group < group_count; group++) {
    int member_count = current->group[group]->member->count;
    for (member = 0; member < member_count; member++) {
      int version_count = current->group[group]->member->member_entry[member].version->count;
      for (version = 0; version < version_count; version++) {
        int local_data_count = current->group[group]->member->member_entry[member].version->local_entry[version].count;
        int *local_data = current->group[group]->member->member_entry[member].version->local_entry[version].data;
        for (local = 0; local < local_data_count; local++) {
          //printf("*** store rank[%d] group[%d] member[%d] local[%d]: %d\n",
          //get_current_rank(*fenix.new_world), group, member, local,
          //local_data[local]);
        }
        int remote_data_count = current->group[group]->member->member_entry[member].version->remote_entry[version].count;
        int *remote_data = current->group[group]->member->member_entry[member].version->remote_entry[version].data;
        for (remote = 0; remote < remote_data_count; remote++) {
          printf("*** store rank[%d] group[%d] member[%d] remote[%d]: %d\n",
                   __fenix_get_current_rank(*fenix.new_world), group, member, remote,
                 remote_data[remote]);
        }
      }
    }
  }
}

/**
 *
 */
void __fenix_dr_print_restore() {
  fenix_data_recovery_t *current = fenix.data_recovery;
  int group_count = current->count;
  int member_count = current->group[0]->member->count;
  int version_count = current->group[0]->member->member_entry[0].version->count;
  int local_data_count = current->group[0]->member->member_entry[0].version->local_entry[0].count;
  int remote_data_count = current->group[0]->member->member_entry[0].version->remote_entry[0].count;
  printf("*** restore rank: %d; group: %d; member: %d; local: %d; remote: %d\n",
           __fenix_get_current_rank(*fenix.new_world), group_count, member_count,
         local_data_count,
         remote_data_count);
}

/**
 *
 */
void __fenix_dr_print_datastructure() {
  int group_index, member_index, version_index, remote_data_index, local_data_index;
  fenix_data_recovery_t *current = fenix.data_recovery;

  if (!current) {
    return;
  }

  printf("\n\ncurrent_rank: %d\n",   __fenix_get_current_rank(*fenix.new_world));
  int group_size = current->total_size;
  for (group_index = 0; group_index < group_size; group_index++) {
    int depth = current->group[group_index]->depth;
    int groupid = current->group[group_index]->groupid;
    int timestamp = current->group[group_index]->timestamp;
    int group_state = current->group[group_index]->state;
    int member_size = current->group[group_index]->member->total_size;
    int member_count = current->group[group_index]->member->count;
    switch (group_state) {
      case EMPTY:
        printf("group[%d] depth: %d groupid: %d timestamp: %d state: %s member.size: %d member.count: %d\n",
               group_index, depth, groupid, timestamp, "EMPTY", member_size,
               member_count);
        break;
      case OCCUPIED:
        printf("group[%d] depth: %d groupid: %d timestamp: %d state: %s member.size: %d member.count: %d\n",
               group_index, depth, groupid, timestamp, "OCCUPIED", member_size,
               member_count);
        break;
      case DELETED:
        printf("group[%d] depth: %d groupid: %d timestamp: %d state: %s member.size: %d member.count: %d\n",
               group_index, depth, groupid, timestamp, "DELETED", member_size,
               member_count);
        break;
      default:
        break;
    }

    for (member_index = 0; member_index < member_size; member_index++) {
      int memberid = current->group[group_index]->member->member_entry[member_index].memberid;
      int member_state = current->group[group_index]->member->member_entry[member_index].state;
      int version_size = current->group[group_index]->member->member_entry[member_index].version->total_size;
      int version_count = current->group[group_index]->member->member_entry[member_index].version->count;
      switch (member_state) {
        case EMPTY:
          printf("group[%d] member[%d] memberid: %d state: %s depth.size: %d depth.count: %d\n",
                 group_index, member_index, memberid, "EMPTY", version_size,
                 version_count);
          break;
        case OCCUPIED:
          printf("group[%d] member[%d] memberid: %d state: %s depth.size: %d depth.count: %d\n",
                 group_index, member_index, memberid, "OCCUPIED", version_size,
                 version_count);
          break;
        case DELETED:
          printf("group[%d] member[%d] memberid: %d state: %s depth.size: %d depth.count: %d\n",
                 group_index, member_index, memberid, "DELETED", version_size,
                 version_count);
          break;
        default:
          break;
      }

      for (version_index = 0; version_index < version_size; version_index++) {
        int local_data_count = current->group[group_index]->member->member_entry[member_index].version->local_entry[version_index].count;
        printf("group[%d] member[%d] version[%d] local_data.count: %d\n",
               group_index,
               member_index,
               version_index, local_data_count);
        if (current->group[group_index]->member->member_entry[member_index].version->local_entry[version_index].data !=
            NULL) {
          int *current_local_data = (int *) current->group[group_index]->member->member_entry[member_index].version->local_entry[version_index].data;
          for (local_data_index = 0; local_data_index < local_data_count; local_data_index++) {
            printf("group[%d] member[%d] depth[%d] local_data[%d]: %d\n",
                   group_index,
                   member_index,
                   version_index, local_data_index,
                   current_local_data[local_data_index]);
          }
        }

        int remote_data_count = current->group[group_index]->member->member_entry[member_index].version->remote_entry[version_index].count;
        printf("group[%d] member[%d] version[%d] remote_data.count: %d\n",
               group_index,
               member_index, version_index, remote_data_count);
        if (current->group[group_index]->member->member_entry[member_index].version->remote_entry[version_index].data !=
            NULL) {
          int *current_remote_data = current->group[group_index]->member->member_entry[member_index].version->remote_entry[version_index].data;
          for (remote_data_index = 0; remote_data_index < remote_data_count; remote_data_index++) {
            printf("group[%d] member[%d] depth[%d] remote_data[%d]: %d\n",
                   group_index,
                   member_index, version_index, remote_data_index,
                   current_remote_data[remote_data_index]);
          }
        }
      }
    }
  }
}
