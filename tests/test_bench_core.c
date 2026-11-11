#include "../bench_core.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static void server_args(void) {
    char *a[] = {"server", "5000", "--bench", "1", "0", "extra"};
    assert(bench_parse_server_args(2,a).value.bench == 0);
    ServerConfigResult r = bench_parse_server_args(4,a);
    assert(r.error == BENCH_OK && r.value.bench && r.value.expected_clients == 1 && r.value.rcvbuf == 0);
    a[3]="100"; a[4]="67108864";
    assert(bench_parse_server_args(5,a).error == BENCH_OK);
    const char *bad[] = {"0","101","-1","abc","1.5",""};
    for (size_t i=0;i<sizeof(bad)/sizeof(*bad);i++) {
        a[3]=(char *)bad[i]; assert(bench_parse_server_args(4,a).error != BENCH_OK);
    }
    a[3]="1"; a[4]="67108865";
    assert(bench_parse_server_args(5,a).error != BENCH_OK);
    a[4]="abc"; assert(bench_parse_server_args(5,a).error != BENCH_OK);
    assert(bench_parse_server_args(3,a).error != BENCH_OK);
    assert(bench_parse_server_args(6,a).error != BENCH_OK);
    a[2]="bad"; assert(bench_parse_server_args(4,a).error != BENCH_OK);
    a[1]="65536"; assert(bench_parse_server_args(2,a).error != BENCH_OK);
}
static void client_args(void) {
    char *a[]={"client","127.0.0.1","5000","--bench","1","1","0","0","extra"};
    assert(bench_parse_client_args(3,a).value.bench == 0);
    ClientConfigResult r=bench_parse_client_args(5,a);
    assert(r.error == BENCH_OK && r.value.bench && r.value.duration_s == 1);
    assert(r.value.block_size == 131072 && r.value.nodelay == 0 && r.value.sndbuf == 0);
    assert(bench_parse_client_args(8,a).error == BENCH_OK);
    a[4]="3600"; a[5]="16777216"; a[6]="1"; a[7]="67108864";
    assert(bench_parse_client_args(8,a).error == BENCH_OK);
    const char *invalid[][4]={{"0","3601","abc","-1"},{"0","16777217","abc","-1"},{"2","-1","abc",""},{"67108865","-1","abc","1.5"}};
    const char *valid[]={"1","131072","0","0"};
    for(int k=0;k<4;k++) for(int j=0;j<4;j++) {
        for(int i=0;i<4;i++) a[4+i]=(char *)valid[i];
        a[4+k]=(char *)invalid[k][j];
        assert(bench_parse_client_args(8,a).error != BENCH_OK);
    }
    assert(bench_parse_client_args(4,a).error != BENCH_OK);
    assert(bench_parse_client_args(9,a).error != BENCH_OK);
    a[3]="bad"; assert(bench_parse_client_args(5,a).error != BENCH_OK);
}
int main(void) {
    assert(fabs(bench_mbps(1250000000,10000000000ULL).value-1000.0)<1e-9);
    assert(bench_mbps(1,0).error == BENCH_ERR_ZERO_TIME);
    assert(bench_mbps(0,1).value == 0);
    assert(bench_mbps(UINT64_MAX,1).error == BENCH_OK);
    uint64_t bytes[]={10,20,30}, ends[]={120,180,150};
    assert(bench_total_bytes(bytes,3)==60);
    assert(bench_window_ns(100,ends,3).value==80);
    assert(bench_window_ns(100,ends,0).error != BENCH_OK);
    ends[1]=99; assert(bench_window_ns(100,ends,3).error != BENCH_OK);
    char out[128];
    assert(bench_format_report(out,sizeof(out),3,1250000000,10000000000ULL,1000)==BENCH_OK);
    assert(strcmp(out,"3,1250000000,10.000,1000.00")==0);
    assert(bench_format_report(out,1,3,1,1,1)==BENCH_ERR_BUFFER);
    assert(bench_parse_long(NULL,0,100).error != BENCH_OK);
    assert(bench_parse_long("999999999999999999999999",0,100).error != BENCH_OK);
    server_args(); client_args();
    puts("CA1–CA5: todos os testes unitários passaram.");
}
