#!/bin/bash

packname=apollo_$(date "+%Y%m%d")

project_dir=$(basename `pwd`)

cd ..

tar jcf $project_dir/${packname}.tar.bz2 $project_dir/bin $project_dir/res $project_dir/uploads

cd -
