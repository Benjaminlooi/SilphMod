package dev.silphmod.nativecheck;

import java.util.*;

final class HuntingBackoff {
    private static final class Entry {long last,until;int failures;}
    private final LinkedHashMap<Long,Entry> areas=new LinkedHashMap<>();
    private static long key(double lat,double lon){return ((long)Math.floor((lat+90)*20)<<32)|(long)Math.floor((lon+180)*20);}
    boolean blocked(double lat,double lon,long now){Entry e=areas.get(key(lat,lon));return e!=null&&now<e.until;}
    void success(double lat,double lon){areas.remove(key(lat,lon));}
    boolean failure(double lat,double lon,long now){
        long key=key(lat,lon);Entry e=areas.computeIfAbsent(key,k->new Entry());if(now-e.last>60000)e.failures=0;e.last=now;
        if(++e.failures>=2)e.until=now+60000;
        while(areas.size()>64){Iterator<Long> i=areas.keySet().iterator();i.next();i.remove();}return now<e.until;
    }
}
