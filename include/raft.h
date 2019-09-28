//
// Created by hww1996 on 2019/9/28.
//

#ifndef TOYRAFT_RAFT_H

class request_vote{
private:
    int term;
    int candidate_id;
    int last_log_term;
    int last_commit_log_index;
};

class request_vote_response{
private:
    int term;
    int vote_for;
};

class request_append{
private:
    
};

#define TOYRAFT_RAFT_H

#endif //TOYRAFT_RAFT_H
