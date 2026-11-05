"""Fixed nondiscard draw, mill and life pipelines using shared zone receipts."""
from hand_exchange_recipes import spec as pipeline_spec, variants as pipeline_variants
from hand_exchange_recipes import expand, validate_witnesses


def spec(d,r):
 return pipeline_spec(d,r,family='value_flow')


def variants(p):
 result=pipeline_variants(p)
 if any(op['kind'] in ('draw','mill') for op in p['operations']):
  result += [f'exact_library_{owner}' for owner in (0,1)]
 if any(op['kind']=='mill' for op in p['operations']) and not any(op['kind']=='draw' for op in p['operations']):
  result += [f'{v}_{owner}' for owner in (0,1) for v in ('short_library','empty_library')]
 return result
