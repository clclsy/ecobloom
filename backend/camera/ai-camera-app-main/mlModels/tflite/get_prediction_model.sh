#!/bin/bash
curl -L -o ./model.tar.gz https://www.kaggle.com/api/v1/models/google/arbitrary-image-stylization-v1/tfLite/256-int8-prediction/1/download
tar -xvf model.tar.gz
rm model.tar.gz
mv 1.tflite arbitrary-image-stylization-v1-predict-int8.tflite

curl -L -o ./model.tar.gz https://www.kaggle.com/api/v1/models/google/arbitrary-image-stylization-v1/tfLite/256-int8-transfer/1/download
tar -xvf model.tar.gz
rm model.tar.gz
mv 1.tflite arbitrary-image-stylization-v1-transfer-int8.tflite

