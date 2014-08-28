for tag in $(git tag | egrep "[0-9]+.[0-9]+.[0-9]+.*"); do
  git tag -d $tag
done
