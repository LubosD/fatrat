#!/bin/bash
# Exercises every /api/v1 endpoint of a running FatRat against the contract in docs/api/openapi.yaml.
# Usage: BASE=http://localhost:9091 PASS=secret tests/rest-smoke.sh
# Needs curl and jq. Leaves one queue named "smoke-2" behind (there is no queue deletion in the API).
# Adds paused magnet transfers to it and deletes them again.
BASE=${BASE:-http://localhost:9091}
PASS=${PASS:-}
API=$BASE/api/v1
fail=0

req() { # method path [curl args...]; sets $status and $body
	local method=$1 path=$2; shift 2
	local out
	out=$(curl -s -w '\n%{http_code}' -u "x:$PASS" -X "$method" "$API$path" "$@")
	status=${out##*$'\n'}
	body=${out%$'\n'*}
}
json() { req "$1" "$2" -H 'Content-Type: application/json' --data "${3:-{\}}"; }
check() { # description expected-status [jq-expression that must be true]
	if [ "$status" != "$2" ]; then echo "FAIL $1: status $status, expected $2: $body"; fail=1; return; fi
	if [ -n "$3" ] && [ "$(echo "$body" | jq -r "$3" 2>/dev/null)" != "true" ]; then echo "FAIL $1: $3 on $body"; fail=1; return; fi
	echo "ok   $1"
}
enc() { jq -rn --arg v "$1" '$v|@uri'; }

req GET /transfer-classes;                       check "transfer-classes" 200 'length > 0 and (.[0]|has("shortName"))'
req GET "/filesystem/directories?path=$(enc /tmp)"; check "list directories" 200 '.path=="/tmp" and (.entries|type=="array") and has("parent")'
req GET "/filesystem/directories";               check "list directories default start" 200 '.path|startswith("/")'
req GET "/filesystem/directories?path=$(enc /nonexistent-smoke)"; check "list directories missing" 404 '.error.code=="path_not_found"'
req GET "/filesystem/directories?path=relative"; check "list directories relative" 422 '.error.code=="invalid_path"'
json POST /queues '{"name":"smoke-test","speedLimits":{"down":10,"up":20}}'
check "create queue" 201 '.name == "smoke-test" and .speedLimits.up == 20'
Q=$(echo "$body" | jq -r .uuid); QE=$(enc "$Q")
req GET /queues;                                 check "list queues" 200 "map(select(.uuid==\"$Q\")) | length == 1"
json PATCH "/queues/$QE" '{"name":"smoke-2","upAsDown":true,"transferLimits":{"down":3,"up":4}}'
check "patch queue" 200 '.name=="smoke-2" and .upAsDown==true and .transferLimits.down==3'
json PATCH "/queues/$QE" '{"name":5}';           check "patch queue bad type" 422 '.error.code=="invalid_property"'
json PATCH "/queues/$QE" '{"bogus":5}';          check "patch queue unknown property" 422
json PATCH "/queues/nope" '{}';                  check "patch unknown queue" 404 '.error.code=="queue_not_found"'

M1='magnet:?xt=urn:btih:0123456789abcdef0123456789abcdef01234567&dn=smoke-one'
M2='magnet:?xt=urn:btih:abcdef0123456789abcdef0123456789abcdef01&dn=smoke-two'
M3='magnet:?xt=urn:btih:1111111111111111111111111111111111111111&dn=smoke-three'
json POST "/queues/$QE/transfers" "{\"urls\":[\"$M1\",\"$M2\",\"$M3\"],\"class\":\"\",\"target\":\"/tmp\",\"paused\":true,\"upload\":false,\"speedLimits\":{\"down\":0,\"up\":0}}"
check "add transfers" 201 '.uuids|length==3'
T1=$(echo "$body" | jq -r '.uuids[0]'); T2=$(echo "$body" | jq -r '.uuids[1]'); T3=$(echo "$body" | jq -r '.uuids[2]')
req GET "/filesystem/directories?queue=$QE"; check "list directories of queue" 200 '.path|startswith("/")'
req GET "/queues/$QE/transfers";                 check "list transfers" 200 'length==3 and .[0].state=="Paused" and (.[0].speeds|has("down"))'
req GET "/transfers/$(enc "$T1")";               check "get transfer" 200 ".uuid==\"$T1\" and .mode==\"Download\""
req GET "/transfers/$(enc "$T1")/details";       check "transfer details" 200 'type=="object"'
req GET "/transfers/$(enc "$T1")/speed-data";    check "transfer speed-data" 200 'type=="array"'
req GET "/queues/$QE/speed-data";                check "queue speed-data" 200 'type=="array"'
req GET "/transfers/nope";                       check "unknown transfer" 404 '.error.code=="transfer_not_found"'

json PATCH "/transfers/$(enc "$T1")" '{"comment":"hello","userSpeedLimits":{"down":5,"up":6}}'; check "patch transfer" 204
req GET "/transfers/$(enc "$T1")";               check "patch applied" 200 '.comment=="hello" and .userSpeedLimits.up==6'
json PATCH /transfers "{\"uuids\":[\"$T1\",\"$T2\"],\"properties\":{\"comment\":\"batch\"}}"; check "batch patch" 204
json PATCH /transfers "{\"uuids\":[\"$T1\",\"$T2\",\"missing\"],\"properties\":{\"comment\":\"never\"}}"; check "batch patch with unknown uuid" 404
json PATCH /transfers "{\"uuids\":[\"$T1\"],\"properties\":{\"comment\":\"never\",\"state\":\"Bogus\"}}"; check "batch patch with bad state" 422
req GET "/transfers/$(enc "$T1")";               check "bad batches changed nothing" 200 '.comment=="batch"'
req GET "/queues/$QE";                           check "queue itself has no GET" 405

order() { req GET "/queues/$QE/transfers"; echo "$body" | jq -r '[.[].uuid] | @csv' | tr -d '"'; }
json POST "/queues/$QE/transfers/move" "{\"uuids\":[\"$T3\"],\"direction\":\"top\"}"; check "move top" 204
[ "$(order)" = "$T3,$T1,$T2" ] && echo "ok   order after top" || { echo "FAIL order after top: $(order)"; fail=1; }
json POST "/queues/$QE/transfers/move" "{\"uuids\":[\"$T3\"],\"direction\":\"down\"}"; check "move down" 204
[ "$(order)" = "$T1,$T3,$T2" ] && echo "ok   order after down" || { echo "FAIL order after down: $(order)"; fail=1; }
json POST "/queues/$QE/transfers/move" "{\"uuids\":[\"$T3\"],\"direction\":\"bottom\"}"; check "move bottom" 204
json POST "/queues/$QE/transfers/move" "{\"uuids\":[\"$T2\"],\"direction\":\"up\"}"; check "move up" 204
[ "$(order)" = "$T2,$T1,$T3" ] && echo "ok   order after bottom+up" || { echo "FAIL order after bottom+up: $(order)"; fail=1; }
json POST "/queues/$QE/transfers/move" "{\"uuids\":[\"$T3\"],\"direction\":\"sideways\"}"; check "move bad direction" 422 '.error.code=="invalid_direction"'
json POST "/queues/$QE/transfers/move" "{\"uuids\":[\"missing\"],\"direction\":\"up\"}"; check "move unknown transfer" 404

req PUT "/transfers/$(enc "$T1")/torrent/file-priorities" -H 'Content-Type: application/json' --data '[{"index":0,"priority":1}]'
[ "$status" = 204 ] || [ "$status" = 409 ] || [ "$status" = 422 ] && echo "ok   file-priorities ($status)" || { echo "FAIL file-priorities: $status $body"; fail=1; }

req GET "/settings?key=$(enc remote/port)&key=$(enc smoke/unset)"; check "get settings" 200 '(.["remote/port"]|tostring) != "null" and .["smoke/unset"]==null'
json PATCH /settings '{"smoke/int":42,"smoke/bool":true,"smoke/str":"x"}'; check "patch settings" 204
req GET "/settings?key=smoke/int&key=smoke/bool&key=smoke/str"; check "settings round trip" 200 '(.["smoke/int"]|tostring)=="42" and (.["smoke/bool"]|tostring)=="true" and .["smoke/str"]=="x"'
req GET /settings;                               check "settings without key" 422
req POST /settings/apply;                        check "apply settings" 202
req GET /settings/pages;                         check "settings pages" 200 'type=="array" and (.[0]|has("script"))'

req DELETE "/transfers?uuid=$(enc "$T1")&uuid=$(enc "$T2")&withData=false"; check "batch delete" 204
req DELETE "/transfers/$(enc "$T3")?withData=true"; check "single delete" 204
req DELETE "/transfers/$(enc "$T3")";            check "delete again" 404
req GET "/queues/$QE/transfers";                 check "queue empty" 200 'length==0'

# protocol-level checks
req POST /queues -H 'Content-Type: text/xml' --data '<x/>';           check "text/xml rejected" 415 '.error.code=="unsupported_media_type"'
req POST /settings/apply -H 'Origin: https://evil.example';           check "foreign origin rejected" 403 '.error.code=="forbidden_origin"'
req POST /queues -H 'Content-Type: application/json' --data '{bad';  check "malformed json" 400 '.error.code=="invalid_json"'
req GET /nothing;                                                     check "unknown route" 404 '.error.code=="unknown_route"'
req DELETE /queues;                                                   check "wrong method" 405
# RSS: feeds, regexps, the test endpoint, the downloaded-episode list and the cascade on feed deletion
req GET /settings/pages;                         check "settings pages list RSS" 200 'map(select(.script=="/scripts/settings/rss.js" and .icon=="/img/settings/rss.png")) | length == 1'
json POST /rss/feeds '{"name":"smoke feed","url":"http://127.0.0.1:9/smoke.rss"}'
check "create feed" 201 '.name=="smoke feed" and (.id|length>0)'
F=$(echo "$body" | jq -r .id); FE=$(enc "$F")
json POST /rss/feeds '{"name":"dup","url":"http://127.0.0.1:9/smoke.rss"}'; check "duplicate feed url" 409 '.error.code=="duplicate_feed"'
json POST /rss/feeds '{"name":"","url":"x"}';    check "feed needs a name" 422 '.error.code=="invalid_property"'
req GET /rss/feeds;                              check "list feeds" 200 "map(select(.id==\"$F\")) | length == 1"
json PATCH "/rss/feeds/$FE" '{"name":"smoke feed 2"}'; check "rename feed" 200 '.name=="smoke feed 2"'
json PATCH "/rss/feeds/nope" '{"name":"x"}';     check "patch unknown feed" 404 '.error.code=="feed_not_found"'
json POST /rss/regexps "{\"feedId\":\"$F\",\"queue\":\"$Q\",\"expression\":\"smoke\",\"tvs\":\"season\"}"
check "create regexp" 201 ".feedId==\"$F\" and .queue==\"$Q\" and .tvs==\"season\" and .from==\"S00E00\" and (.target|length>0)"
R=$(echo "$body" | jq -r .id); RE=$(enc "$R")
json POST /rss/regexps "{\"feedId\":\"$F\",\"queue\":\"$Q\",\"expression\":\"(\"}"; check "invalid regexp" 422 '.error.code=="invalid_property"'
json POST /rss/regexps "{\"feedId\":\"nope\",\"queue\":\"$Q\"}"; check "regexp with unknown feed" 422
json POST /rss/regexps "{\"feedId\":\"$F\",\"queue\":\"nope\"}"; check "regexp with unknown queue" 422
json PATCH "/rss/regexps/$RE" '{"addPaused":true,"to":"S09E99"}'; check "patch regexp" 200 '.addPaused==true and .to=="S09E99" and .expression=="smoke"'
json PATCH "/rss/regexps/$RE" '{"to":"0001"}';   check "bad range for mode" 422
req GET /rss/regexps;                            check "list regexps" 200 "map(select(.id==\"$R\")) | length == 1"
json PUT "/rss/regexps/$RE/downloaded" '["S01E01","S01E02"]'; check "set downloaded" 204
req GET "/rss/regexps/$RE/downloaded";           check "get downloaded" 200 '. == ["S01E01","S01E02"]'
json PUT "/rss/regexps/$RE/downloaded" '["2026-10-16"]'; check "downloaded entry of another mode" 422
json POST /rss/test '{"expression":"smoke.*720p","text":"Smoke Show S01E02 720p","tvs":"season"}'
check "test regexp" 200 '.matches==true and .episode=="S01E02"'
json POST /rss/test '{"expression":"(","text":"x"}'; check "test with invalid regexp" 422
json PATCH "/rss/feeds/$FE" '{"url":"http://127.0.0.1:9/moved.rss"}'; check "change feed url" 200
req GET /rss/regexps;                            check "regexp follows feed url" 200 "map(select(.id==\"$R\" and .feedId==\"$F\")) | length == 1"
req POST /rss/refresh;                           check "refresh" 202
req DELETE "/rss/feeds/$FE";                     check "delete feed" 204
req GET /rss/regexps;                            check "regexps of a deleted feed are gone" 200 "map(select(.id==\"$R\")) | length == 0"
req DELETE "/rss/feeds/$FE";                     check "delete feed twice" 404
req DELETE "/rss/regexps/$RE";                   check "delete cascaded regexp" 404 '.error.code=="regexp_not_found"'

if [ -n "$PASS" ]; then
	status=$(curl -s -o /dev/null -w '%{http_code}' "$API/queues"); [ "$status" = 401 ] && echo "ok   unauthenticated 401" || { echo "FAIL unauthenticated: $status"; fail=1; }
fi
# the server must still be responsive after all the error paths (the old XML-RPC code leaked locks on errors)
req GET /queues; check "still responsive" 200

[ $fail = 0 ] && echo "ALL OK" || echo "FAILURES"
exit $fail
