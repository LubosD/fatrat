#!/bin/sh
echo -- Compiling documentation
cd doc
exec qhelpgenerator fatrat.qhcp -o fatrat.qhc
