#!/bin/sh
echo -- Compiling documentation
cd doc
exec qcollectiongenerator fatrat.qhcp -o fatrat.qhc
