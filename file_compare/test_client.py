import gzip
import requests
 
if __name__ == "__main__":
    url = "http://127.0.0.1:8888"

    resp = requests.get(url + "/task/b9460404-f559-4a93-be01-675045263713")

    # According to header 'Content-Encoding': 'gzip'
    # gzip data is automatically decompressed by requests
    body = resp.content

    # decode body to json
    import json
    print(json.loads(body))


