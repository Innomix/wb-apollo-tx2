#!/bin/bash

cd ..

packname=apollo_$(date "+%Y%m%d")

tar jcf apollo/${packname}.tar.bz2 apollo/bin apollo/res apollo/uploads

cd -
