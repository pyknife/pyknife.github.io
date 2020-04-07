#include <bits/stdc++.h>
using namespace std;

#define TEST

typedef unsigned int ui;
struct Path{
    //ID最小的第一个输出；
    //总体按照循环转账路径长度升序排序；
    //同一级别的路径长度下循环转账账号ID序列，按照字典序（ID转为无符号整数后）升序排序
    int length;
    vector<ui> path;

    Path(int length, const vector<ui> &path) : length(length), path(path) {}

    bool operator<(const Path&rhs)const{
        if(length!=rhs.length) return length<rhs.length;
        for(int i=0;i<length;i++){
            if(path[i]!=rhs.path[i])
                return path[i]<rhs.path[i];
        }
    }
};

class Solution{
public:
    //maxN=560000
    //maxE=280000 ~avgN=26000
    //vector<int> *G;
    vector<vector<int>> G;
    unordered_map<ui,int> idHash; //sorted id to 0...n
    vector<ui> ids; //0...n to sorted id
    vector<ui> inputs; //u-v pairs
    vector<int> inDegrees;
    vector<bool> vis;
    vector<Path> ans;
    int nodeCnt;

    void parseInput(string &testFile){
        FILE* file=fopen(testFile.c_str(),"r");
        ui u,v,c;
        int cnt=0;
        while(fscanf(file,"%u,%u,%u",&u,&v,&c)!=EOF){
            inputs.push_back(u);
            inputs.push_back(v);
            ++cnt;
        }
#ifdef TEST
        printf("%d Records in Total\n",cnt);
#endif
    }

    void constructGraph(){
        auto tmp=inputs;
        sort(tmp.begin(),tmp.end());
        tmp.erase(unique(tmp.begin(),tmp.end()),tmp.end());
        nodeCnt=tmp.size();
        ids=tmp;
        nodeCnt=0;
        for(ui &x:tmp){
            idHash[x]=nodeCnt++;
        }
#ifdef TEST
        printf("%d Nodes in Total\n",nodeCnt);
#endif
        int sz=inputs.size();
        //G=new vector<int>[nodeCnt];
        G=vector<vector<int>>(nodeCnt);
        inDegrees=vector<int>(nodeCnt,0);
        for(int i=0;i<sz;i+=2){
            int u=idHash[inputs[i]],v=idHash[inputs[i+1]];
            G[u].push_back(v);
            ++inDegrees[v];
        }
    }

    void dfs(int head,int cur,int depth,vector<int> &path){
        vis[cur]=true;
        path.push_back(cur);
        for(int &v:G[cur]){
            if(v==head && depth>=3 && depth<=7){
                vector<ui> tmp;
                for(int &x:path)
                    tmp.push_back(ids[x]);
                ans.emplace_back(Path(depth,tmp));
            }
            if(depth<7 && !vis[v] && v>head){
                dfs(head,v,depth+1,path);
            }
        }
        vis[cur]=false;
        path.pop_back();
    }

    //search from 0...n
    //由于要求id最小的在前，因此搜索的全过程中不考虑比起点id更小的节点
    void solve(){
        vis=vector<bool>(nodeCnt,false);
        vector<int> path;
        for(int i=0;i<nodeCnt;i++){
            if(i%100==0)
                cout<<i<<"/"<<nodeCnt<<endl;
            if(!G[i].empty()){
                dfs(i,i,1,path);
            }
        }
        sort(ans.begin(),ans.end());
    }

    void save(string &outputFile){
        printf("Total Loops %d\n",(int)ans.size());
        ofstream out(outputFile);
        out<<ans.size()<<endl;
        for(auto &x:ans){
            auto path=x.path;
            int sz=path.size();
            out<<path[0];
            for(int i=1;i<sz;i++)
                out<<","<<path[i];
            out<<endl;
        }
    }
};

int main()
{
    string testFile = "test_data2.txt";
    string outputFile = "output.txt";
#ifdef TEST
    string answerFile = "result.txt";
#endif
    auto t=clock();
//    for(int i=0;i<100;i++){
        Solution solution;
        solution.parseInput(testFile);
        solution.constructGraph();
        //solution.topoSort();
        solution.solve();
        solution.save(outputFile);
        cout<<clock()-t<<endl;
//    }

    return 0;
}
