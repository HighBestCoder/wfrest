我整理一下：
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