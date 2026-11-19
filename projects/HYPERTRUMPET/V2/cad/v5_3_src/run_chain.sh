set -e
PY=../venv/bin/python
$PY stageH5.py
bash dec_stage.sh
$PY stageS.py
$PY stageD.py
$PY stageL.py
$PY stageF.py
$PY stageG.py
echo CHAIN_DONE
