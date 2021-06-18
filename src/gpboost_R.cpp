/*!
 * Original work Copyright (c) 2017 Microsoft Corporation. All rights reserved.
 * Modified work Copyright (c) 2020 Fabio Sigrist. All rights reserved.
 * Licensed under the Apache License Version 2.0 See LICENSE file in the project root for license information.
 */

#include "gpboost_R.h"

#include <LightGBM/utils/common.h>
#include <LightGBM/utils/log.h>
#include <LightGBM/utils/openmp_wrapper.h>
#include <LightGBM/utils/text_reader.h>

#include <R_ext/Rdynload.h>

#define R_NO_REMAP
#define R_USE_C99_IN_CXX
#include <R_ext/Error.h>

#include <string>
#include <cstdio>
#include <cstring>
#include <memory>
#include <utility>
#include <vector>

#define COL_MAJOR (0)

#define R_API_BEGIN() \
  try {
#define R_API_END() } \
  catch(std::exception& ex) { LGBM_SetLastError(ex.what()); } \
  catch(std::string& ex) { LGBM_SetLastError(ex.c_str()); } \
  catch(...) { LGBM_SetLastError("unknown exception"); } 

#define CHECK_CALL(x) \
  if ((x) != 0) { \
    Rf_error(LGBM_GetLastError()); \
  }

using LightGBM::Common::Split;
using LightGBM::Log;

SEXP LGBM_HandleIsNull_R(SEXP handle) {
	return Rf_ScalarLogical(R_ExternalPtrAddr(handle) == NULL);
}

void _DatasetFinalizer(SEXP handle) {
	LGBM_DatasetFree_R(handle);
}

SEXP LGBM_DatasetCreateFromFile_R(SEXP filename,
	SEXP parameters,
	SEXP reference) {
	int n_protected = 0;
	SEXP ret;
	DatasetHandle handle = nullptr;
	DatasetHandle ref = nullptr;
	if (!Rf_isNull(reference)) {
		ref = R_ExternalPtrAddr(reference);
	}
	const char* filename_ptr = CHAR(PROTECT(Rf_asChar(filename)));
	n_protected++;
	const char* parameters_ptr = CHAR(PROTECT(Rf_asChar(parameters)));
	n_protected++;
	R_API_BEGIN();
	CHECK_CALL(LGBM_DatasetCreateFromFile(filename_ptr, parameters_ptr, ref, &handle));
	R_API_END();
	ret = PROTECT(R_MakeExternalPtr(handle, R_NilValue, R_NilValue));
	n_protected++;
	R_RegisterCFinalizerEx(ret, _DatasetFinalizer, TRUE);
	UNPROTECT(n_protected);
	return ret;
}

SEXP LGBM_DatasetCreateFromCSC_R(SEXP indptr,
	SEXP indices,
	SEXP data,
	SEXP num_indptr,
	SEXP nelem,
	SEXP num_row,
	SEXP parameters,
	SEXP reference) {
	int n_protected = 0;
	SEXP ret;
	const int* p_indptr = INTEGER(indptr);
	const int* p_indices = INTEGER(indices);
	const double* p_data = REAL(data);
	int64_t nindptr = static_cast<int64_t>(Rf_asInteger(num_indptr));
	int64_t ndata = static_cast<int64_t>(Rf_asInteger(nelem));
	int64_t nrow = static_cast<int64_t>(Rf_asInteger(num_row));
	const char* parameters_ptr = CHAR(PROTECT(Rf_asChar(parameters)));
	n_protected++;
	DatasetHandle handle = nullptr;
	DatasetHandle ref = nullptr;
	if (!Rf_isNull(reference)) {
		ref = R_ExternalPtrAddr(reference);
	}
	R_API_BEGIN();
	CHECK_CALL(LGBM_DatasetCreateFromCSC(p_indptr, C_API_DTYPE_INT32, p_indices,
		p_data, C_API_DTYPE_FLOAT64, nindptr, ndata,
		nrow, parameters_ptr, ref, &handle));
	R_API_END();
	ret = PROTECT(R_MakeExternalPtr(handle, R_NilValue, R_NilValue));
	n_protected++;
	R_RegisterCFinalizerEx(ret, _DatasetFinalizer, TRUE);
	UNPROTECT(n_protected);
	return ret;
}

SEXP LGBM_DatasetCreateFromMat_R(SEXP data,
	SEXP num_row,
	SEXP num_col,
	SEXP parameters,
	SEXP reference) {
	int n_protected = 0;
	SEXP ret;
	int32_t nrow = static_cast<int32_t>(Rf_asInteger(num_row));
	int32_t ncol = static_cast<int32_t>(Rf_asInteger(num_col));
	double* p_mat = REAL(data);
	const char* parameters_ptr = CHAR(PROTECT(Rf_asChar(parameters)));
	n_protected++;
	DatasetHandle handle = nullptr;
	DatasetHandle ref = nullptr;
	if (!Rf_isNull(reference)) {
		ref = R_ExternalPtrAddr(reference);
	}
	R_API_BEGIN();
	CHECK_CALL(LGBM_DatasetCreateFromMat(p_mat, C_API_DTYPE_FLOAT64, nrow, ncol, COL_MAJOR,
		parameters_ptr, ref, &handle));
	R_API_END();
	ret = PROTECT(R_MakeExternalPtr(handle, R_NilValue, R_NilValue));
	n_protected++;
	R_RegisterCFinalizerEx(ret, _DatasetFinalizer, TRUE);
	UNPROTECT(n_protected);
	return ret;
}

SEXP LGBM_DatasetGetSubset_R(SEXP handle,
	SEXP used_row_indices,
	SEXP len_used_row_indices,
	SEXP parameters) {
	int n_protected = 0;
	SEXP ret;
	int32_t len = static_cast<int32_t>(Rf_asInteger(len_used_row_indices));
	std::vector<int32_t> idxvec(len);
	// convert from one-based to  zero-based index
#pragma omp parallel for schedule(static, 512) if (len >= 1024)
	for (int32_t i = 0; i < len; ++i) {
		idxvec[i] = static_cast<int32_t>(INTEGER(used_row_indices)[i] - 1);
	}
	const char* parameters_ptr = CHAR(PROTECT(Rf_asChar(parameters)));
	n_protected++;
	DatasetHandle res = nullptr;
	R_API_BEGIN();
	CHECK_CALL(LGBM_DatasetGetSubset(R_ExternalPtrAddr(handle),
		idxvec.data(), len, parameters_ptr,
		&res));
	R_API_END();
	ret = PROTECT(R_MakeExternalPtr(res, R_NilValue, R_NilValue));
	n_protected++;
	R_RegisterCFinalizerEx(ret, _DatasetFinalizer, TRUE);
	UNPROTECT(n_protected);
	return ret;
}

SEXP LGBM_DatasetSetFeatureNames_R(SEXP handle,
	SEXP feature_names) {
	auto vec_names = Split(CHAR(PROTECT(Rf_asChar(feature_names))), '\t');
	std::vector<const char*> vec_sptr;
	int len = static_cast<int>(vec_names.size());
	for (int i = 0; i < len; ++i) {
		vec_sptr.push_back(vec_names[i].c_str());
	}
	R_API_BEGIN();
	CHECK_CALL(LGBM_DatasetSetFeatureNames(R_ExternalPtrAddr(handle),
		vec_sptr.data(), len));
	R_API_END();
	UNPROTECT(1);
	return R_NilValue;
}

SEXP LGBM_DatasetGetFeatureNames_R(SEXP handle) {
	SEXP feature_names;
	int len = 0;
	R_API_BEGIN();
	CHECK_CALL(LGBM_DatasetGetNumFeature(R_ExternalPtrAddr(handle), &len));
	R_API_END();
	const size_t reserved_string_size = 256;
	std::vector<std::vector<char>> names(len);
	std::vector<char*> ptr_names(len);
	for (int i = 0; i < len; ++i) {
		names[i].resize(reserved_string_size);
		ptr_names[i] = names[i].data();
	}
	int out_len;
	size_t required_string_size;
	R_API_BEGIN();
	CHECK_CALL(
		LGBM_DatasetGetFeatureNames(
			R_ExternalPtrAddr(handle),
			len, &out_len,
			reserved_string_size, &required_string_size,
			ptr_names.data()));
	R_API_END();
	// if any feature names were larger than allocated size,
	// allow for a larger size and try again
	if (required_string_size > reserved_string_size) {
		for (int i = 0; i < len; ++i) {
			names[i].resize(required_string_size);
			ptr_names[i] = names[i].data();
		}
		R_API_BEGIN();
		CHECK_CALL(
			LGBM_DatasetGetFeatureNames(
				R_ExternalPtrAddr(handle),
				len,
				&out_len,
				required_string_size,
				&required_string_size,
				ptr_names.data()));
		R_API_END();
	}
	CHECK_EQ(len, out_len);
	feature_names = PROTECT(Rf_allocVector(STRSXP, len));
	for (int i = 0; i < len; ++i) {
		SET_STRING_ELT(feature_names, i, Rf_mkChar(ptr_names[i]));
	}
	UNPROTECT(1);
	return feature_names;
}

SEXP LGBM_DatasetSaveBinary_R(SEXP handle,
	SEXP filename) {
	const char* filename_ptr = CHAR(PROTECT(Rf_asChar(filename)));
	R_API_BEGIN();
	CHECK_CALL(LGBM_DatasetSaveBinary(R_ExternalPtrAddr(handle),
		filename_ptr));
	R_API_END();
	UNPROTECT(1);
	return R_NilValue;
}

SEXP LGBM_DatasetFree_R(SEXP handle) {
	R_API_BEGIN();
	if (!Rf_isNull(handle) && R_ExternalPtrAddr(handle)) {
		CHECK_CALL(LGBM_DatasetFree(R_ExternalPtrAddr(handle)));
		R_ClearExternalPtr(handle);
	}
	R_API_END();
	return R_NilValue;
}

SEXP LGBM_DatasetSetField_R(SEXP handle,
	SEXP field_name,
	SEXP field_data,
	SEXP num_element) {
	int len = Rf_asInteger(num_element);
	const char* name = CHAR(PROTECT(Rf_asChar(field_name)));
	R_API_BEGIN();
	if (!strcmp("group", name) || !strcmp("query", name)) {
		std::vector<int32_t> vec(len);
#pragma omp parallel for schedule(static, 512) if (len >= 1024)
		for (int i = 0; i < len; ++i) {
			vec[i] = static_cast<int32_t>(INTEGER(field_data)[i]);
		}
		CHECK_CALL(LGBM_DatasetSetField(R_ExternalPtrAddr(handle), name, vec.data(), len, C_API_DTYPE_INT32));
	}
	else if (!strcmp("init_score", name)) {
		CHECK_CALL(LGBM_DatasetSetField(R_ExternalPtrAddr(handle), name, REAL(field_data), len, C_API_DTYPE_FLOAT64));
	}
	else {
		std::vector<float> vec(len);
#pragma omp parallel for schedule(static, 512) if (len >= 1024)
		for (int i = 0; i < len; ++i) {
			vec[i] = static_cast<float>(REAL(field_data)[i]);
		}
		CHECK_CALL(LGBM_DatasetSetField(R_ExternalPtrAddr(handle), name, vec.data(), len, C_API_DTYPE_FLOAT32));
	}
	R_API_END();
	UNPROTECT(1);
	return R_NilValue;
}

SEXP LGBM_DatasetGetField_R(SEXP handle,
	SEXP field_name,
	SEXP field_data) {
	const char* name = CHAR(PROTECT(Rf_asChar(field_name)));
	int out_len = 0;
	int out_type = 0;
	const void* res;
	R_API_BEGIN();
	CHECK_CALL(LGBM_DatasetGetField(R_ExternalPtrAddr(handle), name, &out_len, &res, &out_type));
	if (!strcmp("group", name) || !strcmp("query", name)) {
		auto p_data = reinterpret_cast<const int32_t*>(res);
		// convert from boundaries to size
#pragma omp parallel for schedule(static, 512) if (out_len >= 1024)
		for (int i = 0; i < out_len - 1; ++i) {
			INTEGER(field_data)[i] = p_data[i + 1] - p_data[i];
		}
	}
	else if (!strcmp("init_score", name)) {
		auto p_data = reinterpret_cast<const double*>(res);
#pragma omp parallel for schedule(static, 512) if (out_len >= 1024)
		for (int i = 0; i < out_len; ++i) {
			REAL(field_data)[i] = p_data[i];
		}
	}
	else {
		auto p_data = reinterpret_cast<const float*>(res);
#pragma omp parallel for schedule(static, 512) if (out_len >= 1024)
		for (int i = 0; i < out_len; ++i) {
			REAL(field_data)[i] = p_data[i];
		}
	}
	R_API_END();
	UNPROTECT(1);
	return R_NilValue;
}

SEXP LGBM_DatasetGetFieldSize_R(SEXP handle,
	SEXP field_name,
	SEXP out) {
	const char* name = CHAR(PROTECT(Rf_asChar(field_name)));
	int out_len = 0;
	int out_type = 0;
	const void* res;
	R_API_BEGIN();
	CHECK_CALL(LGBM_DatasetGetField(R_ExternalPtrAddr(handle), name, &out_len, &res, &out_type));
	if (!strcmp("group", name) || !strcmp("query", name)) {
		out_len -= 1;
	}
	INTEGER(out)[0] = out_len;
	R_API_END();
	UNPROTECT(1);
	return R_NilValue;
}

SEXP LGBM_DatasetUpdateParamChecking_R(SEXP old_params,
	SEXP new_params) {
	const char* old_params_ptr = CHAR(PROTECT(Rf_asChar(old_params)));
	const char* new_params_ptr = CHAR(PROTECT(Rf_asChar(new_params)));
	R_API_BEGIN();
	CHECK_CALL(LGBM_DatasetUpdateParamChecking(old_params_ptr, new_params_ptr));
	R_API_END();
	UNPROTECT(2);
	return R_NilValue;
}

SEXP LGBM_DatasetGetNumData_R(SEXP handle, SEXP out) {
	int nrow;
	R_API_BEGIN();
	CHECK_CALL(LGBM_DatasetGetNumData(R_ExternalPtrAddr(handle), &nrow));
	INTEGER(out)[0] = nrow;
	R_API_END();
	return R_NilValue;
}

SEXP LGBM_DatasetGetNumFeature_R(SEXP handle,
	SEXP out) {
	int nfeature;
	R_API_BEGIN();
	CHECK_CALL(LGBM_DatasetGetNumFeature(R_ExternalPtrAddr(handle), &nfeature));
	INTEGER(out)[0] = nfeature;
	R_API_END();
	return R_NilValue;
}

// --- start Booster interfaces

void _BoosterFinalizer(SEXP handle) {
	LGBM_BoosterFree_R(handle);
}

SEXP LGBM_BoosterFree_R(SEXP handle) {
	R_API_BEGIN();
	if (!Rf_isNull(handle) && R_ExternalPtrAddr(handle)) {
		CHECK_CALL(LGBM_BoosterFree(R_ExternalPtrAddr(handle)));
		R_ClearExternalPtr(handle);
	}
	R_API_END();
	return R_NilValue;
}

SEXP LGBM_BoosterCreate_R(SEXP train_data,
	SEXP parameters) {
	int n_protected = 0;
	SEXP ret;
	const char* parameters_ptr = CHAR(PROTECT(Rf_asChar(parameters)));
	n_protected++;
	BoosterHandle handle = nullptr;
	R_API_BEGIN();
	CHECK_CALL(LGBM_BoosterCreate(R_ExternalPtrAddr(train_data), parameters_ptr, &handle));
	R_API_END();
	ret = PROTECT(R_MakeExternalPtr(handle, R_NilValue, R_NilValue));
	n_protected++;
	R_RegisterCFinalizerEx(ret, _BoosterFinalizer, TRUE);
	UNPROTECT(n_protected);
	return ret;
}

SEXP LGBM_GPBoosterCreate_R(SEXP train_data,
	SEXP parameters,
	SEXP re_model) {
	int n_protected = 0;
	SEXP ret;
	const char* parameters_ptr = CHAR(PROTECT(Rf_asChar(parameters)));
	n_protected++;
	BoosterHandle handle = nullptr;
	R_API_BEGIN();
	CHECK_CALL(LGBM_GPBoosterCreate(R_ExternalPtrAddr(train_data), parameters_ptr, R_ExternalPtrAddr(re_model), &handle));
	R_API_END();
	ret = PROTECT(R_MakeExternalPtr(handle, R_NilValue, R_NilValue));
	n_protected++;
	R_RegisterCFinalizerEx(ret, _BoosterFinalizer, TRUE);
	UNPROTECT(n_protected);
	return ret;
}

SEXP LGBM_BoosterCreateFromModelfile_R(SEXP filename) {
	int n_protected = 0;
	SEXP ret;
	int out_num_iterations = 0;
	const char* filename_ptr = CHAR(PROTECT(Rf_asChar(filename)));
	n_protected++;
	BoosterHandle handle = nullptr;
	R_API_BEGIN();
	CHECK_CALL(LGBM_BoosterCreateFromModelfile(filename_ptr, &out_num_iterations, &handle));
	R_API_END();
	ret = PROTECT(R_MakeExternalPtr(handle, R_NilValue, R_NilValue));
	n_protected++;
	R_RegisterCFinalizerEx(ret, _BoosterFinalizer, TRUE);
	UNPROTECT(n_protected);
	return ret;
}

SEXP LGBM_BoosterLoadModelFromString_R(SEXP model_str) {
	int n_protected = 0;
	SEXP ret;
	int out_num_iterations = 0;
	const char* model_str_ptr = CHAR(PROTECT(Rf_asChar(model_str)));
	n_protected++;
	BoosterHandle handle = nullptr;
	R_API_BEGIN();
	CHECK_CALL(LGBM_BoosterLoadModelFromString(model_str_ptr, &out_num_iterations, &handle));
	R_API_END();
	ret = PROTECT(R_MakeExternalPtr(handle, R_NilValue, R_NilValue));
	n_protected++;
	R_RegisterCFinalizerEx(ret, _BoosterFinalizer, TRUE);
	UNPROTECT(n_protected);
	return ret;
}

SEXP LGBM_BoosterMerge_R(SEXP handle,
	SEXP other_handle) {
	R_API_BEGIN();
	CHECK_CALL(LGBM_BoosterMerge(R_ExternalPtrAddr(handle), R_ExternalPtrAddr(other_handle)));
	R_API_END();
	return R_NilValue;
}

SEXP LGBM_BoosterAddValidData_R(SEXP handle,
	SEXP valid_data) {
	R_API_BEGIN();
	CHECK_CALL(LGBM_BoosterAddValidData(R_ExternalPtrAddr(handle), R_ExternalPtrAddr(valid_data)));
	R_API_END();
	return R_NilValue;
}

SEXP LGBM_BoosterResetTrainingData_R(SEXP handle,
	SEXP train_data) {
	R_API_BEGIN();
	CHECK_CALL(LGBM_BoosterResetTrainingData(R_ExternalPtrAddr(handle), R_ExternalPtrAddr(train_data)));
	R_API_END();
	return R_NilValue;
}

SEXP LGBM_BoosterResetParameter_R(SEXP handle,
	SEXP parameters) {
	const char* parameters_ptr = CHAR(PROTECT(Rf_asChar(parameters)));
	R_API_BEGIN();
	CHECK_CALL(LGBM_BoosterResetParameter(R_ExternalPtrAddr(handle), parameters_ptr));
	R_API_END();
	UNPROTECT(1);
	return R_NilValue;
}

SEXP LGBM_BoosterGetNumClasses_R(SEXP handle,
	SEXP out) {
	int num_class;
	R_API_BEGIN();
	CHECK_CALL(LGBM_BoosterGetNumClasses(R_ExternalPtrAddr(handle), &num_class));
	INTEGER(out)[0] = num_class;
	R_API_END();
	return R_NilValue;
}

SEXP LGBM_BoosterUpdateOneIter_R(SEXP handle) {
	int is_finished = 0;
	R_API_BEGIN();
	CHECK_CALL(LGBM_BoosterUpdateOneIter(R_ExternalPtrAddr(handle), &is_finished));
	R_API_END();
	return R_NilValue;
}

SEXP LGBM_BoosterUpdateOneIterCustom_R(SEXP handle,
	SEXP grad,
	SEXP hess,
	SEXP len) {
	int is_finished = 0;
	R_API_BEGIN();
	int int_len = Rf_asInteger(len);
	std::vector<float> tgrad(int_len), thess(int_len);
#pragma omp parallel for schedule(static, 512) if (int_len >= 1024)
	for (int j = 0; j < int_len; ++j) {
		tgrad[j] = static_cast<float>(REAL(grad)[j]);
		thess[j] = static_cast<float>(REAL(hess)[j]);
	}
	CHECK_CALL(LGBM_BoosterUpdateOneIterCustom(R_ExternalPtrAddr(handle), tgrad.data(), thess.data(), &is_finished));
	R_API_END();
	return R_NilValue;
}

SEXP LGBM_BoosterRollbackOneIter_R(SEXP handle) {
	R_API_BEGIN();
	CHECK_CALL(LGBM_BoosterRollbackOneIter(R_ExternalPtrAddr(handle)));
	R_API_END();
	return R_NilValue;
}

SEXP LGBM_BoosterGetCurrentIteration_R(SEXP handle,
	SEXP out) {
	int out_iteration;
	R_API_BEGIN();
	CHECK_CALL(LGBM_BoosterGetCurrentIteration(R_ExternalPtrAddr(handle), &out_iteration));
	INTEGER(out)[0] = out_iteration;
	R_API_END();
	return R_NilValue;
}

SEXP LGBM_BoosterGetUpperBoundValue_R(SEXP handle,
	SEXP out_result) {
	R_API_BEGIN();
	double* ptr_ret = REAL(out_result);
	CHECK_CALL(LGBM_BoosterGetUpperBoundValue(R_ExternalPtrAddr(handle), ptr_ret));
	R_API_END();
	return R_NilValue;
}

SEXP LGBM_BoosterGetLowerBoundValue_R(SEXP handle,
	SEXP out_result) {
	R_API_BEGIN();
	double* ptr_ret = REAL(out_result);
	CHECK_CALL(LGBM_BoosterGetLowerBoundValue(R_ExternalPtrAddr(handle), ptr_ret));
	R_API_END();
	return R_NilValue;
}

SEXP LGBM_BoosterGetEvalNames_R(SEXP handle) {
	SEXP eval_names;
	int len;
	R_API_BEGIN();
	CHECK_CALL(LGBM_BoosterGetEvalCounts(R_ExternalPtrAddr(handle), &len));
	R_API_END();
	const size_t reserved_string_size = 128;
	std::vector<std::vector<char>> names(len);
	std::vector<char*> ptr_names(len);
	for (int i = 0; i < len; ++i) {
		names[i].resize(reserved_string_size);
		ptr_names[i] = names[i].data();
	}

	int out_len;
	size_t required_string_size;
	R_API_BEGIN();
	CHECK_CALL(
		LGBM_BoosterGetEvalNames(
			R_ExternalPtrAddr(handle),
			len, &out_len,
			reserved_string_size, &required_string_size,
			ptr_names.data()));
	R_API_END();
	// if any eval names were larger than allocated size,
	// allow for a larger size and try again
	if (required_string_size > reserved_string_size) {
		for (int i = 0; i < len; ++i) {
			names[i].resize(required_string_size);
			ptr_names[i] = names[i].data();
		}
		R_API_BEGIN();
		CHECK_CALL(
			LGBM_BoosterGetEvalNames(
				R_ExternalPtrAddr(handle),
				len,
				&out_len,
				required_string_size,
				&required_string_size,
				ptr_names.data()));
		R_API_END();
	}
	CHECK_EQ(out_len, len);
	eval_names = PROTECT(Rf_allocVector(STRSXP, len));
	for (int i = 0; i < len; ++i) {
		SET_STRING_ELT(eval_names, i, Rf_mkChar(ptr_names[i]));
	}
	UNPROTECT(1);
	return eval_names;
}

SEXP LGBM_BoosterGetEval_R(SEXP handle,
	SEXP data_idx,
	SEXP out_result) {
	R_API_BEGIN();
	int len;
	CHECK_CALL(LGBM_BoosterGetEvalCounts(R_ExternalPtrAddr(handle), &len));
	double* ptr_ret = REAL(out_result);
	int out_len;
	CHECK_CALL(LGBM_BoosterGetEval(R_ExternalPtrAddr(handle), Rf_asInteger(data_idx), &out_len, ptr_ret));
	CHECK_EQ(out_len, len);
	R_API_END();
	return R_NilValue;
}

SEXP LGBM_BoosterGetNumPredict_R(SEXP handle,
	SEXP data_idx,
	SEXP out) {
	R_API_BEGIN();
	int64_t len;
	CHECK_CALL(LGBM_BoosterGetNumPredict(R_ExternalPtrAddr(handle), Rf_asInteger(data_idx), &len));
	INTEGER(out)[0] = static_cast<int>(len);
	R_API_END();
	return R_NilValue;
}

SEXP LGBM_BoosterGetPredict_R(SEXP handle,
	SEXP data_idx,
	SEXP out_result) {
	R_API_BEGIN();
	double* ptr_ret = REAL(out_result);
	int64_t out_len;
	CHECK_CALL(LGBM_BoosterGetPredict(R_ExternalPtrAddr(handle), Rf_asInteger(data_idx), &out_len, ptr_ret));
	R_API_END();
	return R_NilValue;
}

int GetPredictType(SEXP is_rawscore, SEXP is_leafidx, SEXP is_predcontrib) {
	int pred_type = C_API_PREDICT_NORMAL;
	if (Rf_asInteger(is_rawscore)) {
		pred_type = C_API_PREDICT_RAW_SCORE;
	}
	if (Rf_asInteger(is_leafidx)) {
		pred_type = C_API_PREDICT_LEAF_INDEX;
	}
	if (Rf_asInteger(is_predcontrib)) {
		pred_type = C_API_PREDICT_CONTRIB;
	}
	return pred_type;
}

SEXP LGBM_BoosterPredictForFile_R(SEXP handle,
	SEXP data_filename,
	SEXP data_has_header,
	SEXP is_rawscore,
	SEXP is_leafidx,
	SEXP is_predcontrib,
	SEXP start_iteration,
	SEXP num_iteration,
	SEXP parameter,
	SEXP result_filename) {
	const char* data_filename_ptr = CHAR(PROTECT(Rf_asChar(data_filename)));
	const char* parameter_ptr = CHAR(PROTECT(Rf_asChar(parameter)));
	const char* result_filename_ptr = CHAR(PROTECT(Rf_asChar(result_filename)));
	int pred_type = GetPredictType(is_rawscore, is_leafidx, is_predcontrib);
	R_API_BEGIN();
	CHECK_CALL(LGBM_BoosterPredictForFile(R_ExternalPtrAddr(handle), data_filename_ptr,
		Rf_asInteger(data_has_header), pred_type, Rf_asInteger(start_iteration), Rf_asInteger(num_iteration), parameter_ptr,
		result_filename_ptr));
	R_API_END();
	UNPROTECT(3);
	return R_NilValue;
}

SEXP LGBM_BoosterCalcNumPredict_R(SEXP handle,
	SEXP num_row,
	SEXP is_rawscore,
	SEXP is_leafidx,
	SEXP is_predcontrib,
	SEXP start_iteration,
	SEXP num_iteration,
	SEXP out_len) {
	R_API_BEGIN();
	int pred_type = GetPredictType(is_rawscore, is_leafidx, is_predcontrib);
	int64_t len = 0;
	CHECK_CALL(LGBM_BoosterCalcNumPredict(R_ExternalPtrAddr(handle), Rf_asInteger(num_row),
		pred_type, Rf_asInteger(start_iteration), Rf_asInteger(num_iteration), &len));
	INTEGER(out_len)[0] = static_cast<int>(len);
	R_API_END();
	return R_NilValue;
}

SEXP LGBM_BoosterPredictForCSC_R(SEXP handle,
	SEXP indptr,
	SEXP indices,
	SEXP data,
	SEXP num_indptr,
	SEXP nelem,
	SEXP num_row,
	SEXP is_rawscore,
	SEXP is_leafidx,
	SEXP is_predcontrib,
	SEXP start_iteration,
	SEXP num_iteration,
	SEXP parameter,
	SEXP out_result) {
	int pred_type = GetPredictType(is_rawscore, is_leafidx, is_predcontrib);
	const int* p_indptr = INTEGER(indptr);
	const int32_t* p_indices = reinterpret_cast<const int32_t*>(INTEGER(indices));
	const double* p_data = REAL(data);
	int64_t nindptr = static_cast<int64_t>(Rf_asInteger(num_indptr));
	int64_t ndata = static_cast<int64_t>(Rf_asInteger(nelem));
	int64_t nrow = static_cast<int64_t>(Rf_asInteger(num_row));
	double* ptr_ret = REAL(out_result);
	int64_t out_len;
	const char* parameter_ptr = CHAR(PROTECT(Rf_asChar(parameter)));
	R_API_BEGIN();
	CHECK_CALL(LGBM_BoosterPredictForCSC(R_ExternalPtrAddr(handle),
		p_indptr, C_API_DTYPE_INT32, p_indices,
		p_data, C_API_DTYPE_FLOAT64, nindptr, ndata,
		nrow, pred_type, Rf_asInteger(start_iteration), Rf_asInteger(num_iteration), parameter_ptr, &out_len, ptr_ret));
	R_API_END();
	UNPROTECT(1);
	return R_NilValue;
}

SEXP LGBM_BoosterPredictForMat_R(SEXP handle,
	SEXP data,
	SEXP num_row,
	SEXP num_col,
	SEXP is_rawscore,
	SEXP is_leafidx,
	SEXP is_predcontrib,
	SEXP start_iteration,
	SEXP num_iteration,
	SEXP parameter,
	SEXP out_result) {
	int pred_type = GetPredictType(is_rawscore, is_leafidx, is_predcontrib);
	int32_t nrow = static_cast<int32_t>(Rf_asInteger(num_row));
	int32_t ncol = static_cast<int32_t>(Rf_asInteger(num_col));
	const double* p_mat = REAL(data);
	double* ptr_ret = REAL(out_result);
	const char* parameter_ptr = CHAR(PROTECT(Rf_asChar(parameter)));
	int64_t out_len;
	R_API_BEGIN();
	CHECK_CALL(LGBM_BoosterPredictForMat(R_ExternalPtrAddr(handle),
		p_mat, C_API_DTYPE_FLOAT64, nrow, ncol, COL_MAJOR,
		pred_type, Rf_asInteger(start_iteration), Rf_asInteger(num_iteration), parameter_ptr, &out_len, ptr_ret));
	R_API_END();
	UNPROTECT(1);
	return R_NilValue;
}

SEXP LGBM_BoosterSaveModel_R(SEXP handle,
	SEXP num_iteration,
	SEXP feature_importance_type,
	SEXP filename) {
	const char* filename_ptr = CHAR(PROTECT(Rf_asChar(filename)));
	R_API_BEGIN();
	CHECK_CALL(LGBM_BoosterSaveModel(R_ExternalPtrAddr(handle), 0, Rf_asInteger(num_iteration), Rf_asInteger(feature_importance_type), filename_ptr));
	R_API_END();
	UNPROTECT(1);
	return R_NilValue;
}

SEXP LGBM_BoosterSaveModelToString_R(SEXP handle,
	SEXP start_iteration,
	SEXP num_iteration,
	SEXP feature_importance_type) {
	SEXP model_str;
	int64_t out_len = 0;
	int64_t buf_len = 1024 * 1024;
	int start_iter = Rf_asInteger(start_iteration);
	int num_iter = Rf_asInteger(num_iteration);
	int importance_type = Rf_asInteger(feature_importance_type);
	std::vector<char> inner_char_buf(buf_len);
	R_API_BEGIN();
	CHECK_CALL(LGBM_BoosterSaveModelToString(R_ExternalPtrAddr(handle), start_iter, num_iter, importance_type, buf_len, &out_len, inner_char_buf.data()));
	R_API_END();
	// if the model string was larger than the initial buffer, allocate a bigger buffer and try again
	if (out_len > buf_len) {
		inner_char_buf.resize(out_len);
		R_API_BEGIN();
		CHECK_CALL(LGBM_BoosterSaveModelToString(R_ExternalPtrAddr(handle), start_iter, num_iter, importance_type, out_len, &out_len, inner_char_buf.data()));
		R_API_END();
	}
	model_str = PROTECT(Rf_allocVector(STRSXP, 1));
	SET_STRING_ELT(model_str, 0, Rf_mkChar(inner_char_buf.data()));
	UNPROTECT(1);
	return model_str;
}

SEXP LGBM_BoosterDumpModel_R(SEXP handle,
	SEXP num_iteration,
	SEXP feature_importance_type) {
	SEXP model_str;
	int64_t out_len = 0;
	int64_t buf_len = 1024 * 1024;
	int num_iter = Rf_asInteger(num_iteration);
	int importance_type = Rf_asInteger(feature_importance_type);
	std::vector<char> inner_char_buf(buf_len);
	R_API_BEGIN();
	CHECK_CALL(LGBM_BoosterDumpModel(R_ExternalPtrAddr(handle), 0, num_iter, importance_type, buf_len, &out_len, inner_char_buf.data()));
	R_API_END();
	// if the model string was larger than the initial buffer, allocate a bigger buffer and try again
	if (out_len > buf_len) {
		inner_char_buf.resize(out_len);
		R_API_BEGIN();
		CHECK_CALL(LGBM_BoosterDumpModel(R_ExternalPtrAddr(handle), 0, num_iter, importance_type, out_len, &out_len, inner_char_buf.data()));
		R_API_END();
	}
	model_str = PROTECT(Rf_allocVector(STRSXP, 1));
	SET_STRING_ELT(model_str, 0, Rf_mkChar(inner_char_buf.data()));
	UNPROTECT(1);
	return model_str;
}

// Below here are REModel / GPModel related functions

void _REModelFinalizer(SEXP handle) {
	GPB_REModelFree_R(handle);
}

SEXP GPB_CreateREModel_R(SEXP ndata,
	SEXP cluster_ids_data,
	SEXP re_group_data,
	SEXP num_re_group,
	SEXP re_group_rand_coef_data,
	SEXP ind_effect_group_rand_coef,
	SEXP num_re_group_rand_coef,
	SEXP num_gp,
	SEXP gp_coords_data,
	SEXP dim_gp_coords,
	SEXP gp_rand_coef_data,
	SEXP num_gp_rand_coef,
	SEXP cov_fct,
	SEXP cov_fct_shape,
	SEXP cov_fct_taper_range,
	SEXP vecchia_approx,
	SEXP num_neighbors,
	SEXP vecchia_ordering,
	SEXP vecchia_pred_type,
	SEXP num_neighbors_pred,
	SEXP likelihood) {
	int n_protected = 0;
	SEXP ret;
	REModelHandle handle = nullptr;
	SEXP cov_fct_aux = PROTECT(Rf_asChar(cov_fct));
	SEXP vecchia_ordering_aux = PROTECT(Rf_asChar(vecchia_ordering));
	SEXP vecchia_pred_type_aux = PROTECT(Rf_asChar(vecchia_pred_type));
	SEXP likelihood_aux = PROTECT(Rf_asChar(likelihood));
	n_protected = n_protected + 4;
	const char* cov_fct_ptr = (Rf_isNull(cov_fct)) ? nullptr : CHAR(cov_fct_aux);
	const char* vecchia_ordering_ptr = (Rf_isNull(vecchia_ordering)) ? nullptr : CHAR(vecchia_ordering_aux);
	const char* vecchia_pred_type_ptr = (Rf_isNull(vecchia_pred_type)) ? nullptr : CHAR(vecchia_pred_type_aux);
	const char* likelihood_ptr = (Rf_isNull(likelihood)) ? nullptr : CHAR(likelihood_aux);
	R_API_BEGIN();
	CHECK_CALL(GPB_CreateREModel(Rf_asInteger(ndata),
		R_INT_PTR(cluster_ids_data),
		R_CHAR_PTR_FROM_RAW(re_group_data),
		Rf_asInteger(num_re_group),
		R_REAL_PTR(re_group_rand_coef_data),
		R_INT_PTR(ind_effect_group_rand_coef),
		Rf_asInteger(num_re_group_rand_coef),
		Rf_asInteger(num_gp),
		R_REAL_PTR(gp_coords_data),
		Rf_asInteger(dim_gp_coords),
		R_REAL_PTR(gp_rand_coef_data),
		Rf_asInteger(num_gp_rand_coef),
		cov_fct_ptr,
		Rf_asReal(cov_fct_shape),
		Rf_asReal(cov_fct_taper_range),
		Rf_asLogical(vecchia_approx),
		Rf_asInteger(num_neighbors),
		vecchia_ordering_ptr,
		vecchia_pred_type_ptr,
		Rf_asInteger(num_neighbors_pred),
		likelihood_ptr,
		&handle));
	R_API_END();
	ret = PROTECT(R_MakeExternalPtr(handle, R_NilValue, R_NilValue));
	n_protected++;
	R_RegisterCFinalizerEx(ret, _REModelFinalizer, TRUE);
	UNPROTECT(n_protected);
	return ret;
}

SEXP GPB_REModelFree_R(SEXP handle) {
	R_API_BEGIN();
	if (R_ExternalPtrAddr(handle) != nullptr) {
		CHECK_CALL(GPB_REModelFree(R_ExternalPtrAddr(handle)));
		R_ClearExternalPtr(handle);
	}
	R_API_END();
	return R_NilValue;
}

SEXP GPB_SetOptimConfig_R(SEXP handle,
	SEXP init_cov_pars,
	SEXP lr,
	SEXP acc_rate_cov,
	SEXP max_iter,
	SEXP delta_rel_conv,
	SEXP use_nesterov_acc,
	SEXP nesterov_schedule_version,
	SEXP trace,
	SEXP optimizer,
	SEXP momentum_offset,
	SEXP convergence_criterion,
	SEXP calc_std_dev) {
	int n_protected = 0;
	SEXP optimizer_aux = PROTECT(Rf_asChar(optimizer));
	SEXP convergence_criterion_aux = PROTECT(Rf_asChar(convergence_criterion));
	n_protected = n_protected + 2;
	const char* optimizer_ptr = (Rf_isNull(optimizer)) ? nullptr : CHAR(optimizer_aux);
	const char* convergence_criterion_ptr = (Rf_isNull(convergence_criterion)) ? nullptr : CHAR(convergence_criterion_aux);
	R_API_BEGIN();
	CHECK_CALL(GPB_SetOptimConfig(R_ExternalPtrAddr(handle),
		R_REAL_PTR(init_cov_pars),
		Rf_asReal(lr),
		Rf_asReal(acc_rate_cov),
		Rf_asInteger(max_iter),
		Rf_asReal(delta_rel_conv),
		Rf_asLogical(use_nesterov_acc),
		Rf_asInteger(nesterov_schedule_version),
		Rf_asLogical(trace),
		optimizer_ptr,
		Rf_asInteger(momentum_offset),
		convergence_criterion_ptr,
		Rf_asLogical(calc_std_dev)));
	R_API_END();
	UNPROTECT(n_protected);
	return R_NilValue;
}

SEXP GPB_SetOptimCoefConfig_R(SEXP handle,
	SEXP num_covariates,
	SEXP init_coef,
	SEXP lr_coef,
	SEXP acc_rate_coef,
	SEXP optimizer) {
	int n_protected = 0;
	SEXP optimizer_aux = PROTECT(Rf_asChar(optimizer));
	n_protected++;
	const char* optimizer_ptr = (Rf_isNull(optimizer)) ? nullptr : CHAR(optimizer_aux);
	R_API_BEGIN();
	CHECK_CALL(GPB_SetOptimCoefConfig(R_ExternalPtrAddr(handle),
		Rf_asInteger(num_covariates),
		R_REAL_PTR(init_coef),
		Rf_asReal(lr_coef),
		Rf_asReal(acc_rate_coef),
		optimizer_ptr));
	R_API_END();
	UNPROTECT(n_protected);
	return R_NilValue;
}

SEXP GPB_OptimCovPar_R(SEXP handle,
	SEXP y_data,
	SEXP fixed_effects) {
	R_API_BEGIN();
	CHECK_CALL(GPB_OptimCovPar(R_ExternalPtrAddr(handle),
		R_REAL_PTR(y_data),
		R_REAL_PTR(fixed_effects)));
	R_API_END();
	return R_NilValue;
}

SEXP GPB_OptimLinRegrCoefCovPar_R(SEXP handle,
	SEXP y_data,
	SEXP covariate_data,
	SEXP num_covariates) {
	R_API_BEGIN();
	CHECK_CALL(GPB_OptimLinRegrCoefCovPar(R_ExternalPtrAddr(handle),
		R_REAL_PTR(y_data),
		R_REAL_PTR(covariate_data),
		Rf_asInteger(num_covariates)));
	R_API_END();
	return R_NilValue;
}

SEXP GPB_EvalNegLogLikelihood_R(SEXP handle,
	SEXP y_data,
	SEXP cov_pars,
	SEXP negll) {
	R_API_BEGIN();
	CHECK_CALL(GPB_EvalNegLogLikelihood(R_ExternalPtrAddr(handle),
		R_REAL_PTR(y_data),
		R_REAL_PTR(cov_pars),
		R_REAL_PTR(negll)));
	R_API_END();
	return R_NilValue;
}

SEXP GPB_GetCovPar_R(SEXP handle,
	SEXP calc_std_dev,
	SEXP optim_cov_pars) {
	R_API_BEGIN();
	CHECK_CALL(GPB_GetCovPar(R_ExternalPtrAddr(handle),
		R_REAL_PTR(optim_cov_pars),
		Rf_asLogical(calc_std_dev)));
	R_API_END();
	return R_NilValue;
}

SEXP GPB_GetInitCovPar_R(SEXP handle,
	SEXP init_cov_pars) {
	R_API_BEGIN();
	CHECK_CALL(GPB_GetInitCovPar(R_ExternalPtrAddr(handle),
		R_REAL_PTR(init_cov_pars)));
	R_API_END();
	return R_NilValue;
}

SEXP GPB_GetCoef_R(SEXP handle,
	SEXP calc_std_dev,
	SEXP optim_coef) {
	R_API_BEGIN();
	CHECK_CALL(GPB_GetCoef(R_ExternalPtrAddr(handle),
		R_REAL_PTR(optim_coef),
		Rf_asLogical(calc_std_dev)));
	R_API_END();
	return R_NilValue;
}

SEXP GPB_GetNumIt_R(SEXP handle,
	SEXP num_it) {
	R_API_BEGIN();
	CHECK_CALL(GPB_GetNumIt(R_ExternalPtrAddr(handle),
		R_INT_PTR(num_it)));
	R_API_END();
	return R_NilValue;
}

SEXP GPB_SetPredictionData_R(SEXP handle,
	SEXP num_data_pred,
	SEXP cluster_ids_data_pred,
	SEXP re_group_data_pred,
	SEXP re_group_rand_coef_data_pred,
	SEXP gp_coords_data_pred,
	SEXP gp_rand_coef_data_pred,
	SEXP covariate_data_pred) {
	R_API_BEGIN();
	CHECK_CALL(GPB_SetPredictionData(R_ExternalPtrAddr(handle),
		Rf_asInteger(num_data_pred),
		R_INT_PTR(cluster_ids_data_pred),
		R_CHAR_PTR_FROM_RAW(re_group_data_pred),
		R_REAL_PTR(re_group_rand_coef_data_pred),
		R_REAL_PTR(gp_coords_data_pred),
		R_REAL_PTR(gp_rand_coef_data_pred),
		R_REAL_PTR(covariate_data_pred)));
	R_API_END();
	return R_NilValue;
}

SEXP GPB_PredictREModel_R(SEXP handle,
	SEXP y_data,
	SEXP num_data_pred,
	SEXP predict_cov_mat,
	SEXP predict_var,
	SEXP predict_response,
	SEXP cluster_ids_data_pred,
	SEXP re_group_data_pred,
	SEXP re_group_rand_coef_data_pred,
	SEXP gp_coords_pred,
	SEXP gp_rand_coef_data_pred,
	SEXP cov_pars,
	SEXP covariate_data_pred,
	SEXP use_saved_data,
	SEXP vecchia_pred_type,
	SEXP num_neighbors_pred,
	SEXP fixed_effects,
	SEXP fixed_effects_pred,
	SEXP out_predict) {
	int n_protected = 0;
	SEXP vecchia_pred_type_aux = PROTECT(Rf_asChar(vecchia_pred_type));
	n_protected++;
	const char* vecchia_pred_type_ptr = (Rf_isNull(vecchia_pred_type)) ? nullptr : CHAR(vecchia_pred_type_aux);
	R_API_BEGIN();
	CHECK_CALL(GPB_PredictREModel(R_ExternalPtrAddr(handle),
		R_REAL_PTR(y_data),
		Rf_asInteger(num_data_pred),
		R_REAL_PTR(out_predict),
		Rf_asLogical(predict_cov_mat),
		Rf_asLogical(predict_var),
		Rf_asLogical(predict_response),
		R_INT_PTR(cluster_ids_data_pred),
		R_CHAR_PTR_FROM_RAW(re_group_data_pred),
		R_REAL_PTR(re_group_rand_coef_data_pred),
		R_REAL_PTR(gp_coords_pred),
		R_REAL_PTR(gp_rand_coef_data_pred),
		R_REAL_PTR(cov_pars),
		R_REAL_PTR(covariate_data_pred),
		Rf_asLogical(use_saved_data),
		vecchia_pred_type_ptr,
		Rf_asInteger(num_neighbors_pred),
		R_REAL_PTR(fixed_effects),
		R_REAL_PTR(fixed_effects_pred)));
	R_API_END();
	UNPROTECT(n_protected);
	return R_NilValue;
}

SEXP GPB_GetLikelihoodName_R(SEXP handle) {
	SEXP ret;
	std::vector<char> inner_char_buf(128);
	int num_char;
	R_API_BEGIN();
	CHECK_CALL(GPB_GetLikelihoodName(R_ExternalPtrAddr(handle),
		inner_char_buf.data(),
		num_char));
	R_API_END();
	ret = PROTECT(Rf_allocVector(STRSXP, 1));
	SET_STRING_ELT(ret, 0, Rf_mkChar(inner_char_buf.data()));
	UNPROTECT(1);
	return ret;
}

SEXP GPB_GetOptimizerCovPars_R(SEXP handle) {
	SEXP ret;
	std::vector<char> inner_char_buf(128);
	int num_char;
	R_API_BEGIN();
	CHECK_CALL(GPB_GetOptimizerCovPars(R_ExternalPtrAddr(handle),
		inner_char_buf.data(),
		num_char));
	R_API_END();
	ret = PROTECT(Rf_allocVector(STRSXP, 1));
	SET_STRING_ELT(ret, 0, Rf_mkChar(inner_char_buf.data()));
	UNPROTECT(1);
	return ret;
}

SEXP GPB_GetOptimizerCoef_R(SEXP handle) {
	SEXP ret;
	std::vector<char> inner_char_buf(128);
	int num_char;
	R_API_BEGIN();
	CHECK_CALL(GPB_GetOptimizerCoef(R_ExternalPtrAddr(handle),
		inner_char_buf.data(),
		num_char));
	R_API_END();
	ret = PROTECT(Rf_allocVector(STRSXP, 1));
	SET_STRING_ELT(ret, 0, Rf_mkChar(inner_char_buf.data()));
	UNPROTECT(1);
	return ret;
}

SEXP GPB_SetLikelihood_R(SEXP handle,
	SEXP likelihood) {
	int n_protected = 0;
	SEXP likelihood_aux = PROTECT(Rf_asChar(likelihood));
	n_protected++;
	const char* likelihood_ptr = (Rf_isNull(likelihood)) ? nullptr : CHAR(likelihood_aux);
	R_API_BEGIN();
	CHECK_CALL(GPB_SetLikelihood(R_ExternalPtrAddr(handle),
		likelihood_ptr));
	R_API_END();
	UNPROTECT(n_protected);
	return R_NilValue;
}

SEXP GPB_GetResponseData_R(SEXP handle,
	SEXP response_data) {
	R_API_BEGIN();
	CHECK_CALL(GPB_GetResponseData(R_ExternalPtrAddr(handle),
		R_REAL_PTR(response_data)));
	R_API_END();
	return R_NilValue;
}

SEXP GPB_GetCovariateData_R(SEXP handle,
	SEXP covariate_data) {
	R_API_BEGIN();
	CHECK_CALL(GPB_GetCovariateData(R_ExternalPtrAddr(handle),
		R_REAL_PTR(covariate_data)));
	R_API_END();
	return R_NilValue;
}

// .Call() calls
static const R_CallMethodDef CallEntries[] = {
  {"LGBM_HandleIsNull_R"              , (DL_FUNC)&LGBM_HandleIsNull_R              , 1},
  {"LGBM_DatasetCreateFromFile_R"     , (DL_FUNC)&LGBM_DatasetCreateFromFile_R     , 3},
  {"LGBM_DatasetCreateFromCSC_R"      , (DL_FUNC)&LGBM_DatasetCreateFromCSC_R      , 8},
  {"LGBM_DatasetCreateFromMat_R"      , (DL_FUNC)&LGBM_DatasetCreateFromMat_R      , 5},
  {"LGBM_DatasetGetSubset_R"          , (DL_FUNC)&LGBM_DatasetGetSubset_R          , 4},
  {"LGBM_DatasetSetFeatureNames_R"    , (DL_FUNC)&LGBM_DatasetSetFeatureNames_R    , 2},
  {"LGBM_DatasetGetFeatureNames_R"    , (DL_FUNC)&LGBM_DatasetGetFeatureNames_R    , 1},
  {"LGBM_DatasetSaveBinary_R"         , (DL_FUNC)&LGBM_DatasetSaveBinary_R         , 2},
  {"LGBM_DatasetFree_R"               , (DL_FUNC)&LGBM_DatasetFree_R               , 1},
  {"LGBM_DatasetSetField_R"           , (DL_FUNC)&LGBM_DatasetSetField_R           , 4},
  {"LGBM_DatasetGetFieldSize_R"       , (DL_FUNC)&LGBM_DatasetGetFieldSize_R       , 3},
  {"LGBM_DatasetGetField_R"           , (DL_FUNC)&LGBM_DatasetGetField_R           , 3},
  {"LGBM_DatasetUpdateParamChecking_R", (DL_FUNC)&LGBM_DatasetUpdateParamChecking_R, 2},
  {"LGBM_DatasetGetNumData_R"         , (DL_FUNC)&LGBM_DatasetGetNumData_R         , 2},
  {"LGBM_DatasetGetNumFeature_R"      , (DL_FUNC)&LGBM_DatasetGetNumFeature_R      , 2},
  {"LGBM_BoosterCreate_R"             , (DL_FUNC)&LGBM_BoosterCreate_R             , 2},
  {"LGBM_GPBoosterCreate_R"           , (DL_FUNC)&LGBM_GPBoosterCreate_R           , 3},
  {"LGBM_BoosterFree_R"               , (DL_FUNC)&LGBM_BoosterFree_R               , 1},
  {"LGBM_BoosterCreateFromModelfile_R", (DL_FUNC)&LGBM_BoosterCreateFromModelfile_R, 1},
  {"LGBM_BoosterLoadModelFromString_R", (DL_FUNC)&LGBM_BoosterLoadModelFromString_R, 1},
  {"LGBM_BoosterMerge_R"              , (DL_FUNC)&LGBM_BoosterMerge_R              , 2},
  {"LGBM_BoosterAddValidData_R"       , (DL_FUNC)&LGBM_BoosterAddValidData_R       , 2},
  {"LGBM_BoosterResetTrainingData_R"  , (DL_FUNC)&LGBM_BoosterResetTrainingData_R  , 2},
  {"LGBM_BoosterResetParameter_R"     , (DL_FUNC)&LGBM_BoosterResetParameter_R     , 2},
  {"LGBM_BoosterGetNumClasses_R"      , (DL_FUNC)&LGBM_BoosterGetNumClasses_R      , 2},
  {"LGBM_BoosterUpdateOneIter_R"      , (DL_FUNC)&LGBM_BoosterUpdateOneIter_R      , 1},
  {"LGBM_BoosterUpdateOneIterCustom_R", (DL_FUNC)&LGBM_BoosterUpdateOneIterCustom_R, 4},
  {"LGBM_BoosterRollbackOneIter_R"    , (DL_FUNC)&LGBM_BoosterRollbackOneIter_R    , 1},
  {"LGBM_BoosterGetCurrentIteration_R", (DL_FUNC)&LGBM_BoosterGetCurrentIteration_R, 2},
  {"LGBM_BoosterGetUpperBoundValue_R" , (DL_FUNC)&LGBM_BoosterGetUpperBoundValue_R , 2},
  {"LGBM_BoosterGetLowerBoundValue_R" , (DL_FUNC)&LGBM_BoosterGetLowerBoundValue_R , 2},
  {"LGBM_BoosterGetEvalNames_R"       , (DL_FUNC)&LGBM_BoosterGetEvalNames_R       , 1},
  {"LGBM_BoosterGetEval_R"            , (DL_FUNC)&LGBM_BoosterGetEval_R            , 3},
  {"LGBM_BoosterGetNumPredict_R"      , (DL_FUNC)&LGBM_BoosterGetNumPredict_R      , 3},
  {"LGBM_BoosterGetPredict_R"         , (DL_FUNC)&LGBM_BoosterGetPredict_R         , 3},
  {"LGBM_BoosterPredictForFile_R"     , (DL_FUNC)&LGBM_BoosterPredictForFile_R     , 10},
  {"LGBM_BoosterCalcNumPredict_R"     , (DL_FUNC)&LGBM_BoosterCalcNumPredict_R     , 8},
  {"LGBM_BoosterPredictForCSC_R"      , (DL_FUNC)&LGBM_BoosterPredictForCSC_R      , 14},
  {"LGBM_BoosterPredictForMat_R"      , (DL_FUNC)&LGBM_BoosterPredictForMat_R      , 11},
  {"LGBM_BoosterSaveModel_R"          , (DL_FUNC)&LGBM_BoosterSaveModel_R          , 4},
  {"LGBM_BoosterSaveModelToString_R"  , (DL_FUNC)&LGBM_BoosterSaveModelToString_R  , 4},
  {"LGBM_BoosterDumpModel_R"          , (DL_FUNC)&LGBM_BoosterDumpModel_R          , 3},
  {"GPB_CreateREModel_R"              , (DL_FUNC)&GPB_CreateREModel_R              , 21},
  {"GPB_REModelFree_R"                , (DL_FUNC)&GPB_REModelFree_R                , 1},
  {"GPB_SetOptimConfig_R"             , (DL_FUNC)&GPB_SetOptimConfig_R             , 13},
  {"GPB_SetOptimCoefConfig_R"         , (DL_FUNC)&GPB_SetOptimCoefConfig_R         , 6},
  {"GPB_OptimCovPar_R"                , (DL_FUNC)&GPB_OptimCovPar_R                , 3},
  {"GPB_OptimLinRegrCoefCovPar_R"     , (DL_FUNC)&GPB_OptimLinRegrCoefCovPar_R     , 4},
  {"GPB_EvalNegLogLikelihood_R"       , (DL_FUNC)&GPB_EvalNegLogLikelihood_R       , 4},
  {"GPB_GetCovPar_R"                  , (DL_FUNC)&GPB_GetCovPar_R                  , 3},
  {"GPB_GetInitCovPar_R"              , (DL_FUNC)&GPB_GetInitCovPar_R              , 2},
  {"GPB_GetCoef_R"                    , (DL_FUNC)&GPB_GetCoef_R                    , 3},
  {"GPB_GetNumIt_R"                   , (DL_FUNC)&GPB_GetNumIt_R                   , 2},
  {"GPB_SetPredictionData_R"          , (DL_FUNC)&GPB_SetPredictionData_R          , 8},
  {"GPB_PredictREModel_R"             , (DL_FUNC)&GPB_PredictREModel_R             , 19},
  {"GPB_GetLikelihoodName_R"          , (DL_FUNC)&GPB_GetLikelihoodName_R          , 1},
  {"GPB_GetOptimizerCovPars_R"        , (DL_FUNC)&GPB_GetOptimizerCovPars_R        , 1},
  {"GPB_GetOptimizerCoef_R"           , (DL_FUNC)&GPB_GetOptimizerCoef_R           , 1},
  {"GPB_SetLikelihood_R"              , (DL_FUNC)&GPB_SetLikelihood_R              , 2},
  {"GPB_GetResponseData_R"            , (DL_FUNC)&GPB_GetResponseData_R            , 2},
  {"GPB_GetCovariateData_R"           , (DL_FUNC)&GPB_GetCovariateData_R           , 2},
  {NULL, NULL, 0}
};

LIGHTGBM_C_EXPORT void R_init_gpboost(DllInfo* dll);

void R_init_gpboost(DllInfo* dll) {
	R_registerRoutines(dll, NULL, CallEntries, NULL, NULL);
	R_useDynamicSymbols(dll, FALSE);
}
