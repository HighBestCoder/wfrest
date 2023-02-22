# 运行

```
docker build . -t file_compare
docker run -idt -p 8888:8888 file_compare /opt/fc/file_compare/main
```

# 发送任务

URL:

```
http://ip:8888/task/b9460404-f559-4a93-be01-675045263713
```

method: "POST"
Header:
- "Content-Encoding", "gzip"
- "Content-Type", "application/json"
body:

```
{
  "uuid": "b9460404-f559-4a93-be01-675045263713",
  "result_type": 0,
  "servers": [
    {
      "center": "m1",
      "host": "localhost",
      "user": "admin",
      "password": "admin",
      "port": 8091,
      "standard": true,
      "path_to_compare": "/opt"
    }
  ]
}
```

Response:

```
{
	"msg": "put into queue, wait for schedule to run",
	"uuid": "b9460404-f559-4a93-be01-675045263713"
}
```

# 请求任务状态


URL:

```
http://ip:8888/task/b9460404-f559-4a93-be01-675045263713
```

method: "GET"
Header:
- "Content-Encoding", "gzip"
- "Content-Type", "application/json"

Response:

```
 {
 	"diffs": [{
 		"dir": "",
 		"name": "opt",
 		"servers": [{
 			"is_standard": true,
 			"last_updated": "2023022190516",
 			"md5": "",
 			"mode": "755",
 			"owner": "root",
 			"server_name": "m1",
 			"size": 0
 		}]
 	}, {
 		"dir": "/opt",
 		"name": "fc",
 		"servers": [{
 			"is_standard": true,
 			"last_updated": "2023022220933",
 			"md5": "",
 			"mode": "755",
 			"owner": "root",
 			"server_name": "m1",
 			"size": 0
 		}]
 	}, {
 		"dir": "/opt/fc",
 		"name": "Dockerfile",
 		"servers": [{
 			"is_standard": true,
 			"last_updated": "2023022220932",
 			"md5": "39d1d743f915c4df7c726c1505338931",
 			"mode": "644",
 			"owner": "root",
 			"server_name": "m1",
 			"size": 201
 		}]
 	}, {
 		"dir": "/opt/fc",
 		"name": "_lib",
 		"servers": [{
 			"is_standard": true,
 			"last_updated": "2023022220933",
 			"md5": "",
 			"mode": "755",
 			"owner": "root",
 			"server_name": "m1",
 			"size": 0
 		}]
 	}, {
 		"dir": "/opt/fc/_lib",
 		"name": "workflow",
 		"servers": [{
 			"is_standard": true,
 			"last_updated": "2023022192701",
 			"md5": "",
 			"mode": "755",
 			"owner": "root",
 			"server_name": "m1",
 			"size": 0
 		}]
 	}, {
 		"dir": "/opt/fc/_lib/workflow",
 		"name": "_lib",
 		"servers": [{
 			"is_standard": true,
 			"last_updated": "2023022220933",
 			"md5": "",
 			"mode": "755",
 			"owner": "root",
 			"server_name": "m1",
 			"size": 0
 		}]
 	}, {
 		"dir": "/opt/fc/_lib/workflow/_lib",
 		"name": "libworkflow.so",
 		"servers": [{
 			"is_standard": true,
 			"last_updated": "2023022220932",
 			"md5": "c8313d91ef01084bb7c51e85ea254032",
 			"mode": "755",
 			"owner": "root",
 			"server_name": "m1",
 			"size": 7757192
 		}]
 	}, {
 		"dir": "/opt/fc/_lib",
 		"name": "libwfrest.so",
 		"servers": [{
 			"is_standard": true,
 			"last_updated": "2023022220932",
 			"md5": "e70004cabcf088ed933bb7f76fbcde9e",
 			"mode": "644",
 			"owner": "root",
 			"server_name": "m1",
 			"size": 60
 		}]
 	}, {
 		"dir": "/opt/fc",
 		"name": "workflow",
 		"servers": [{
 			"is_standard": true,
 			"last_updated": "2023022192701",
 			"md5": "",
 			"mode": "755",
 			"owner": "root",
 			"server_name": "m1",
 			"size": 0
 		}]
 	}, {
 		"dir": "/opt/fc/workflow",
 		"name": "_lib",
 		"servers": [{
 			"is_standard": true,
 			"last_updated": "2023022220933",
 			"md5": "",
 			"mode": "755",
 			"owner": "root",
 			"server_name": "m1",
 			"size": 0
 		}]
 	}, {
 		"dir": "/opt/fc/workflow/_lib",
 		"name": "libworkflow.so.0",
 		"servers": [{
 			"is_standard": true,
 			"last_updated": "2023022220932",
 			"md5": "c8313d91ef01084bb7c51e85ea254032",
 			"mode": "755",
 			"owner": "root",
 			"server_name": "m1",
 			"size": 7757192
 		}]
 	}, {
 		"dir": "/opt/fc",
 		"name": "file_compare",
 		"servers": [{
 			"is_standard": true,
 			"last_updated": "2023022220933",
 			"md5": "",
 			"mode": "755",
 			"owner": "root",
 			"server_name": "m1",
 			"size": 0
 		}]
 	}, {
 		"dir": "/opt/fc/file_compare",
 		"name": "main",
 		"servers": [{
 			"is_standard": true,
 			"last_updated": "2023022220932",
 			"md5": "e1d54cc1584acdc4bcef0d4a88755edd",
 			"mode": "755",
 			"owner": "root",
 			"server_name": "m1",
 			"size": 10780000
 		}]
 	}, {
 		"dir": "/opt/fc/file_compare",
 		"name": "post",
 		"servers": [{
 			"is_standard": true,
 			"last_updated": "2023022220932",
 			"md5": "8268946779475bc0bb5dd7a55b740df8",
 			"mode": "755",
 			"owner": "root",
 			"server_name": "m1",
 			"size": 2053000
 		}]
 	}, {
 		"dir": "/opt/fc/file_compare",
 		"name": "README.md",
 		"servers": [{
 			"is_standard": true,
 			"last_updated": "2023022220932",
 			"md5": "c075a2bbba6d155a979d6aab6401f3b8",
 			"mode": "644",
 			"owner": "root",
 			"server_name": "m1",
 			"size": 1735
 		}]
 	}, {
 		"dir": "/opt/fc/file_compare",
 		"name": "get",
 		"servers": [{
 			"is_standard": true,
 			"last_updated": "2023022220932",
 			"md5": "fbfbfd37a027c2d517b23ef8ce2853d1",
 			"mode": "755",
 			"owner": "root",
 			"server_name": "m1",
 			"size": 2053000
 		}]
 	}, {
 		"dir": "/opt/fc/file_compare",
 		"name": "input.json",
 		"servers": [{
 			"is_standard": true,
 			"last_updated": "2023022220932",
 			"md5": "e44e3feac6eae26901be36c15219eaad",
 			"mode": "644",
 			"owner": "root",
 			"server_name": "m1",
 			"size": 280
 		}]
 	}],
 	"id": "b9460404-f559-4a93-be01-675045263713",
 	"next_shard": -1
 }
```





# 返回值 

1. 所有的目录 & 文件都要返回标准方的属性。并且文件都需要有md5值。
2. 如果属性里面一样的，约定就不返回。
3. 如果有出错，带上出错码/出错信息。
4. 如果某个数据中心，与标准方的文件XX/目录XX属性/内容完全一样。那么不需要写XX在这个数据中心的属性。
5. 带上任务信息。
6. 比基准方多出来的文件也是需要显示在差异列表里面。

文件属性：size，permission, owner, md5, errno, error_msg, diff_file_path,last_updated_time, comment.

文件夹属性：是permission, owner和last_updated_time, errno, error_msg

NOTE: 其中文件的[errno, error_msg, diff_file_path, comment]，目录的[errno, error_msg]不一定有


```Json
{
 "id": "task_id",
 "next_shard": 1221,
 "diff": [{
   "name": "a_a.txt",
   "dir": "/A",
   "servers": [{
     "is_standard": true,
     "server_name": "datacenter1",
     "permission": "xxx",
     "owner": "xxx",
     "last_updated_time": "xxx"
    },
    {
     "server_name": "datacenter2",
     "permission": "DD",
     "owner": "DD",
     "last_updated_time": "DD"
    }
   ]
  },
  {
   "name": "b_a.txt",
   "dir": "/A/B",
   "servers": [{
     "is_standard": true,
     "server_name": "datacenter1",
     "permission": "xxx",
     "owner": "xxx",
     "last_updated_time": "xxx"
    },
    {
     "server_name": "datacenter2",
     "permission": "xxx",
     "owner": "xxx",
     "last_updated_time": "xxx"
    }
   ]
  },
  {
   "name": "/A/B/C",
   "is_dir": true,
   "servers": [{
     "is_standard": true,
     "server_name": "datacenter1",
     "owner": "xxx"
    },
    {
     "server_name": "datacenter2",
     "owner": "yyy"
    }
   ]
  }
 ]
}
```