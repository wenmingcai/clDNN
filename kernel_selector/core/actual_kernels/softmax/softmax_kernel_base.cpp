﻿/*
// Copyright (c) 2016 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
*/

#include "softmax_kernel_base.h"

namespace KernelSelector 
{
    JitConstants SoftmaxKernelBase::GetJitConstants(const SoftmaxParams& params, SoftmaxKernelBase::DispatchData kd) const
    {
        JitConstants mem_consts = MakeSoftmaxJitConstants(params);

        mem_consts.AddConstants({
            MakeJitConstant("ITEMS_NUM",      kd.itemsNum),
            MakeJitConstant("LWS",            kd.lws0),
            MakeJitConstant("GWS",            kd.gws0),
            MakeJitConstant("DATA_SETS_COUNT",kd.dataSetsCount),
            MakeJitConstant("DATA_SET_SIZE",  kd.dataSetSize),
            MakeJitConstant("LEFTOVERS",      kd.leftovers),
        });

        return mem_consts;
    }

    SoftmaxKernelBase::DispatchData SoftmaxKernelBase::SetDefault(const SoftmaxParams& params, const OptionalParams&) const
    {
        DispatchData runInfo;

        runInfo.gws0 = 1;
        runInfo.gws1 = 1;
        runInfo.gws2 = 1;

        runInfo.lws0 = 1;
        runInfo.lws1 = 1;
        runInfo.lws2 = 1;


        runInfo.fp16UnitUsed = params.inputs[0].GetDType() == Datatype::F16;
        runInfo.leftovers = 0;
        runInfo.itemsNum = 0;
        runInfo.normIndex = 0;
        runInfo.dataSetsCount = 0;
        runInfo.dataSetSize = 0;

        return runInfo;
    }

    bool SoftmaxKernelBase::Validate(const Params& p, const OptionalParams& o) const
    {
        if (p.GetType() != KernelType::SOFT_MAX ||
            o.GetType() != KernelType::SOFT_MAX)
        {
            return false;
        }

        return true;
    }

    KernelsData SoftmaxKernelBase::GetCommonKernelsData(const Params& params, const OptionalParams& options) const
    {
        if (!Validate(params, options))
        {
            return{};
        }

        const SoftmaxParams& orgParams = static_cast<const SoftmaxParams&>(params);
        KernelData kd = KernelData::Default<SoftmaxParams>(params);

        auto runInfo = SetDefault(orgParams, options);
        auto cldnn_jit = GetJitConstants(orgParams, runInfo);
        auto entry_point = GetEntryPoint(kernelName, orgParams.layerID, options);
        auto jit = CreateJit(kernelName, cldnn_jit, entry_point);

        auto& kernel = kd.kernels[0];
        FillCLKernelData(kernel, runInfo, kernelName, jit, entry_point);

        kd.estimatedTime = runInfo.effiency;

        return{ kd };
    }

    bool SoftmaxKernelBaseBF::Validate(const Params& p, const OptionalParams& o) const
    {
        if (!Parent::Validate(p, o))
        {
            return false;
        }

        const SoftmaxParams& params = static_cast<const SoftmaxParams&>(p);
        const auto& input = params.inputs[0];

        if (params.activationFunc != ActivationFunction::NONE)
        {
            return false;
        }

        if (input.GetLayout() == DataLayout::bf ||
            input.GetLayout() == DataLayout::fb)
        {
            return true;
        }

        switch (params.smParams.dim)
        {
        case SoftmaxDim::X:         return input.Y().v == 1 && input.Feature().v == 1;
        case SoftmaxDim::Y:         return input.X().v == 1 && input.Feature().v == 1;
        case SoftmaxDim::FEATURE:   return input.X().v == 1 && input.Y().v == 1;
        default:                    return false;
        }
    }

    SoftmaxKernelBase::DispatchData SoftmaxKernelBaseBF::SetDefault(const SoftmaxParams& params, const OptionalParams& options) const
    {
        const auto& input = params.inputs[0];

        DispatchData kd = Parent::SetDefault(params, options);

        auto flatten_input = input.FlattenFeatureAndSpatials();
        kd.dataSetSize = flatten_input.Feature().v;
        kd.dataSetsCount = input.Batch().v;

        return kd;
    }
}