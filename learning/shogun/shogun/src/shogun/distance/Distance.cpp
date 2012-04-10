/*
 * this program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * Written (W) 2006-2009 Christian Gehl
 * Written (W) 2006-2009 Soeren Sonnenburg
 * Copyright (C) 2006-2009 Fraunhofer Institute FIRST and Max-Planck-Society
 */

#include <shogun/lib/config.h>
#include <shogun/lib/common.h>
#include <shogun/io/SGIO.h>
#include <shogun/io/File.h>
#include <shogun/lib/Time.h>
#include <shogun/base/Parallel.h>
#include <shogun/base/Parameter.h>

#include <shogun/distance/Distance.h>
#include <shogun/features/Features.h>

#include <string.h>
#include <unistd.h>

#ifndef WIN32
#include <pthread.h>
#endif

using namespace shogun;

#ifndef DOXYGEN_SHOULD_SKIP_THIS
struct D_THREAD_PARAM
{
	CDistance* dist;
	float64_t* matrix;
	int32_t idx_start;
	int32_t idx_stop;
	int32_t idx_step;
};
#endif /* DOXYGEN_SHOULD_SKIP_THIS */

CDistance::CDistance() : CSGObject()
{
	init();
}

		
CDistance::CDistance(CFeatures* p_lhs, CFeatures* p_rhs) : CSGObject()
{
	init();
	init(p_lhs, p_rhs);
}

CDistance::~CDistance()
{
	SG_FREE(precomputed_matrix);
	precomputed_matrix=NULL;

	remove_lhs_and_rhs();
}

bool CDistance::init(CFeatures* l, CFeatures* r)
{
	//make sure features were indeed supplied
	ASSERT(l);
	ASSERT(r);

	//make sure features are compatible
	ASSERT(l->get_feature_class()==r->get_feature_class());
	ASSERT(l->get_feature_type()==r->get_feature_type());

	//remove references to previous features
	remove_lhs_and_rhs();

	//increase reference counts
    	SG_REF(l);
	SG_REF(r);

	lhs=l;
	rhs=r;

	SG_FREE(precomputed_matrix);
	precomputed_matrix=NULL ;

	return true;
}

void CDistance::load(CFile* loader)
{
	SG_SET_LOCALE_C;
	SG_RESET_LOCALE;
}

void CDistance::save(CFile* writer)
{
	SG_SET_LOCALE_C;
	SG_RESET_LOCALE;
}

void CDistance::remove_lhs_and_rhs()
{
	SG_UNREF(rhs);
	rhs = NULL;

	SG_UNREF(lhs);
	lhs = NULL;
}

void CDistance::remove_lhs()
{ 
	SG_UNREF(lhs);
	lhs = NULL;
}

/// takes all necessary steps if the rhs is removed from kernel
void CDistance::remove_rhs()
{
	SG_UNREF(rhs);
	rhs = NULL;
}

CFeatures* CDistance::replace_rhs(CFeatures* r)
{
     //make sure features were indeed supplied
     ASSERT(r);

     //make sure features are compatible
     ASSERT(lhs->get_feature_class()==r->get_feature_class());
     ASSERT(lhs->get_feature_type()==r->get_feature_type());

     //remove references to previous rhs features
     CFeatures* tmp=rhs;
     
     rhs=r;

     SG_FREE(precomputed_matrix);
     precomputed_matrix=NULL ;

	 // return old features including reference count
     return tmp;
}

void CDistance::do_precompute_matrix()
{
	int32_t num_left=lhs->get_num_vectors();
	int32_t num_right=rhs->get_num_vectors();
	SG_INFO( "precomputing distance matrix (%ix%i)\n", num_left, num_right) ;

	ASSERT(num_left==num_right);
	ASSERT(lhs==rhs);
	int32_t num=num_left;
	
	SG_FREE(precomputed_matrix);
	precomputed_matrix=SG_MALLOC(float32_t, num*(num+1)/2);

	for (int32_t i=0; i<num; i++)
	{
		SG_PROGRESS(i*i,0,num*num);
		for (int32_t j=0; j<=i; j++)
			precomputed_matrix[i*(i+1)/2+j] = compute(i,j) ;
	}

	SG_PROGRESS(num*num,0,num*num);
	SG_DONE();
}

SGMatrix<float64_t> CDistance::get_distance_matrix()
{
	int32_t m,n;
	float64_t* data=get_distance_matrix_real(m,n,NULL);
	return SGMatrix<float64_t>(data, m,n);
}

float32_t* CDistance::get_distance_matrix_shortreal(
	int32_t &num_vec1, int32_t &num_vec2, float32_t* target)
{
	float32_t* result = NULL;
	CFeatures* f1 = lhs;
	CFeatures* f2 = rhs;

	if (f1 && f2)
	{
		if (target && (num_vec1!=f1->get_num_vectors() || num_vec2!=f2->get_num_vectors()))
			SG_ERROR("distance matrix does not fit into target\n");

		num_vec1=f1->get_num_vectors();
		num_vec2=f2->get_num_vectors();
		int64_t total_num=num_vec1*num_vec2;
		int32_t num_done=0;

		SG_DEBUG("returning distance matrix of size %dx%d\n", num_vec1, num_vec2);

		if (target)
			result=target;
		else
			result=SG_MALLOC(float32_t, total_num);

		if ( (f1 == f2) && (num_vec1 == num_vec2) )
		{
			for (int32_t i=0; i<num_vec1; i++)
			{
				for (int32_t j=i; j<num_vec1; j++)
				{
					float64_t v=distance(i,j);

					result[i+j*num_vec1]=v;
					result[j+i*num_vec1]=v;

					if (num_done%100000)
						SG_PROGRESS(num_done, 0, total_num-1);

					if (i!=j)
						num_done+=2;
					else
						num_done+=1;
				}
			}
		}
		else
		{
			for (int32_t i=0; i<num_vec1; i++)
			{
				for (int32_t j=0; j<num_vec2; j++)
				{
					result[i+j*num_vec1]=distance(i,j) ;

					if (num_done%100000)
						SG_PROGRESS(num_done, 0, total_num-1);

					num_done++;
				}
			}
		}

		SG_DONE();
	}
	else
      		SG_ERROR("no features assigned to distance\n");

	return result;
}

float64_t* CDistance::get_distance_matrix_real(
	int32_t &num_vec1, int32_t &num_vec2, float64_t* target)
{
	float64_t* result = NULL;
	CFeatures* f1 = lhs;
	CFeatures* f2 = rhs;

	if (f1 && f2)
	{
		if (target && (num_vec1!=f1->get_num_vectors() || num_vec2!=f2->get_num_vectors()))
			SG_ERROR("distance matrix does not fit into target\n");

		num_vec1=f1->get_num_vectors();
		num_vec2=f2->get_num_vectors();
		int64_t total_num=num_vec1*num_vec2;
		int32_t num_done=0;

		SG_DEBUG("returning distance matrix of size %dx%d\n", num_vec1, num_vec2);

		if (target)
			result=target;
		else
			result=SG_MALLOC(float64_t, total_num);

		if ( (f1 == f2) && (num_vec1 == num_vec2) )
		{
		#ifndef WIN32
			// twice CPU?
			int32_t num_threads = 2*parallel->get_num_threads();
			ASSERT(num_threads>0);
			pthread_t* threads = SG_MALLOC(pthread_t, num_threads);
			D_THREAD_PARAM* parameters = SG_MALLOC(D_THREAD_PARAM,num_threads);
			pthread_attr_t attr;
			pthread_attr_init(&attr);
			pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
			for (int32_t t=0; t<num_threads; t++)
			{
				parameters[t].idx_start = t;
				parameters[t].idx_stop = num_vec1;
				parameters[t].idx_step = num_threads;
				parameters[t].matrix = result;
				parameters[t].dist = this;
				
				pthread_create(&threads[t], &attr, CDistance::run_distance_thread, (void*)&parameters[t]);
			}
			for (int32_t t=0; t<num_threads; t++)
			{
				pthread_join(threads[t], NULL);
			}
			pthread_attr_destroy(&attr);
			
			SG_FREE(parameters);
			SG_FREE(threads);
		#else
			for (int32_t i=0; i<num_vec1; i++)
			{
				for (int32_t j=i; j<num_vec1; j++)
				{
					float64_t v=distance(i,j);

					result[i+j*num_vec1]=v;
					result[j+i*num_vec1]=v;

					if (num_done%100000)
						SG_PROGRESS(num_done, 0, total_num-1);

					if (i!=j)
						num_done+=2;
					else
						num_done+=1;
				}
			}
		#endif
		}
		else
		{
			for (int32_t i=0; i<num_vec1; i++)
			{
				for (int32_t j=0; j<num_vec2; j++)
				{
					result[i+j*num_vec1]=distance(i,j) ;

					if (num_done%100000)
						SG_PROGRESS(num_done, 0, total_num-1);

					num_done++;
				}
			}
		}

		SG_DONE();
	}
	else
      		SG_ERROR("no features assigned to distance\n");

	return result;
}

void CDistance::init()
{
	precomputed_matrix = NULL;
	precompute_matrix = false;
	lhs = NULL;
	rhs = NULL;

	m_parameters->add((CSGObject**) &lhs, "lhs",
					  "Feature vectors to occur on left hand side.");
	m_parameters->add((CSGObject**) &rhs, "rhs",
					  "Feature vectors to occur on right hand side.");
}

void* CDistance::run_distance_thread(void* p)
{
	D_THREAD_PARAM* parameters = (D_THREAD_PARAM*)p;
	float64_t* matrix = parameters->matrix;
	CDistance* dist = parameters->dist;
	int32_t idx_start = parameters->idx_start;
	int32_t idx_stop = parameters->idx_stop;
	int32_t idx_step = parameters->idx_step;
	for (int32_t i=idx_start; i<idx_stop; i+=idx_step)
	{
		for (int32_t j=i; j<idx_stop; j++)
		{
			float64_t ij_dist = dist->compute(i,j);
			matrix[i*idx_stop+j] = ij_dist;
			matrix[j*idx_stop+i] = ij_dist;
		}
	}
	return NULL;
}
