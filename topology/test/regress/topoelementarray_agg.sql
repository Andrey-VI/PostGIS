set client_min_messages to WARNING;

SELECT 1,topology.TopoElementArray_agg('{1,3}'::topology.TopoElement);

SELECT 2, topology.TopoElementArray_agg(e) from (
 select '{2,4}'::topology.TopoElement as e union
 select '{2,5}'
) as foo;
